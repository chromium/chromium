// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/cross_device_signin_promo_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/callback_list.h"
#include "base/containers/enum_set.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_ui_util.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace {

// Per-surface impression limit for dismissible promos.
constexpr int kMaxShownCount = 5;

using CrossDevicePromoSurface = SigninPrefs::CrossDevicePromoSurface;
using CrossDevicePromoState = SigninPrefs::CrossDevicePromoState;
using CrossDevicePromoSurfaces =
    base::EnumSet<CrossDevicePromoSurface,
                  CrossDevicePromoSurface::kHistory,
                  CrossDevicePromoSurface::kMaxValue>;

std::string_view GetEntryPointHistogramSuffix(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  // LINT.IfChange(CrossDeviceSigninPromoEntryPointVariant)
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      return "ForeignTabs";
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return "HistoryPage";
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
      return "WhatsNewPage";
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return "ProfileMenu";
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return "SendTabToSelf";
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return "BookmarkSave";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/signin/histograms.xml:CrossDeviceSigninPromoEntryPointVariant)
}

void RecordShouldShowResult(CrossDeviceSigninPromoEntryPoint entry_point,
                            CrossDeviceSigninPromoShouldShowResult result) {
  base::UmaHistogramEnumeration(
      base::StrCat({"Signin.CrossDeviceSigninPromo.ShouldShowResult.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      result);
}

// Returns the primary account's GaiaId. Expects the profile to be signed in
// with a valid non-empty GaiaId (CHECK-enforced).
GaiaId GetPrimaryAccountGaiaId(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CHECK(identity_manager);
  GaiaId gaia_id =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  CHECK(!gaia_id.empty());
  return gaia_id;
}

// Returns std::nullopt for non-dismissible promo entry points.
std::optional<CrossDevicePromoSurface> GetPromoSurfaceForEntryPoint(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return CrossDevicePromoSurface::kHistory;
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      return std::nullopt;
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return CrossDevicePromoSurface::kBookmarkSave;
  }
}

void WriteDismissiblePromoData(Profile* profile,
                               CrossDeviceSigninPromoEntryPoint entry_point,
                               const CrossDevicePromoState& data) {
  std::optional<CrossDevicePromoSurface> surface =
      GetPromoSurfaceForEntryPoint(entry_point);
  if (!surface.has_value()) {
    NOTREACHED() << "Entry point does not have any promo data";
  }
  SigninPrefs(*profile->GetPrefs())
      .SetCrossDevicePromoState(GetPrimaryAccountGaiaId(profile), *surface,
                                data);
}

// Returns std::nullopt for non-dismissible promo entry points, and
// empty/defaulted data if the promo state is not set yet.
std::optional<CrossDevicePromoState> GetPromoDataForDismissibleEntryPoint(
    Profile* profile,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  std::optional<CrossDevicePromoSurface> surface =
      GetPromoSurfaceForEntryPoint(entry_point);
  if (!surface.has_value()) {
    return std::nullopt;
  }
  return SigninPrefs(*profile->GetPrefs())
      .GetCrossDevicePromoState(GetPrimaryAccountGaiaId(profile), *surface);
}

// Returns whether an explicit dismissal on any surface started the cooldown
// shared by all surfaces, and that cooldown is still active.
bool IsDismissiblePromoCooldownActive(const SigninPrefs& signin_prefs,
                                      const GaiaId& gaia_id) {
  static constexpr base::TimeDelta kDismissiblePromoCooldownPeriod =
      base::Days(7);
  std::optional<base::Time> last_dismissed_time;
  for (CrossDevicePromoSurface surface : CrossDevicePromoSurfaces::All()) {
    const CrossDevicePromoState state =
        signin_prefs.GetCrossDevicePromoState(gaia_id, surface);
    if (state.last_dismissed_time.has_value()) {
      last_dismissed_time = std::max(last_dismissed_time.value_or(base::Time()),
                                     *state.last_dismissed_time);
    }
  }
  return last_dismissed_time.has_value() &&
         base::Time::Now() <
             *last_dismissed_time + kDismissiblePromoCooldownPeriod;
}

// Checks the frequency caps for the dismissible promo on `surface`:
// - Each surface can be shown at most 5 times, and all surfaces at most 6
//   times in total.
// - Each surface is suppressed after 2 explicit dismissals, and all surfaces
//   after 4 explicit dismissals in total.
// - Cooldown period of 7 days across all surfaces after an explicit dismissal.
// Records no metrics.
CrossDeviceSigninPromoShouldShowResult CheckDismissiblePromoLimits(
    Profile* profile,
    CrossDevicePromoSurface surface) {
  static constexpr int kMaxTotalShownCount = 6;
  static constexpr int kMaxDismissedCount = 2;
  static constexpr int kMaxTotalDismissedCount = 4;

  const GaiaId gaia_id = GetPrimaryAccountGaiaId(profile);
  SigninPrefs signin_prefs(*profile->GetPrefs());
  const CrossDevicePromoState state =
      signin_prefs.GetCrossDevicePromoState(gaia_id, surface);

  int total_shown_count = 0;
  int total_dismissed_count = 0;
  for (CrossDevicePromoSurface other_surface :
       CrossDevicePromoSurfaces::All()) {
    const CrossDevicePromoState other_state =
        signin_prefs.GetCrossDevicePromoState(gaia_id, other_surface);
    total_shown_count =
        base::ClampAdd(total_shown_count, other_state.shown_count);
    total_dismissed_count =
        base::ClampAdd(total_dismissed_count, other_state.dismissed_count);
  }

  if (total_dismissed_count >= kMaxTotalDismissedCount) {
    return CrossDeviceSigninPromoShouldShowResult::kGlobalDismissalLimitReached;
  }
  if (state.shown_count >= kMaxShownCount) {
    return CrossDeviceSigninPromoShouldShowResult::kShownLimitReached;
  }
  if (total_shown_count >= kMaxTotalShownCount) {
    return CrossDeviceSigninPromoShouldShowResult::kGlobalShownLimitReached;
  }
  if (state.dismissed_count >= kMaxDismissedCount) {
    return CrossDeviceSigninPromoShouldShowResult::kDismissalLimitReached;
  }
  if (IsDismissiblePromoCooldownActive(signin_prefs, gaia_id)) {
    return CrossDeviceSigninPromoShouldShowResult::kCooldownActive;
  }
  return CrossDeviceSigninPromoShouldShowResult::kCanShow;
}

bool IsUserSignedInWithNoError(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin_util::SignedInState state =
      signin_util::GetSignedInState(identity_manager);
  return state == signin_util::SignedInState::kSignedIn ||
         state == signin_util::SignedInState::kSyncing;
}

bool HasMobileDevice(Profile* profile) {
  syncer::DeviceInfoSyncService* device_info_sync_service =
      DeviceInfoSyncServiceFactory::GetForProfile(profile);
  CHECK(device_info_sync_service);
  syncer::DeviceInfoTracker* device_info_tracker =
      device_info_sync_service->GetDeviceInfoTracker();
  CHECK(device_info_tracker);

  return std::ranges::any_of(
      device_info_tracker->GetAllChromeDeviceInfo(),
      [device_info_tracker](const syncer::DeviceInfo* device_info) {
        if (device_info_tracker->IsRecentLocalCacheGuid(device_info->guid())) {
          return false;
        }
        return device_info->form_factor() ==
               syncer::DeviceInfo::FormFactor::kPhone;
      });
}

bool IsSyncTypeEnabled(Profile* profile, syncer::UserSelectableType type) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(type);
}

std::string_view GetCrossDevicePromoCampaign(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return "XDeviceProfileMenu";
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return "XDeviceHistoryPage";
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return "XDeviceSendTabToSelf";
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
      // TODO(crbug.com/571014322): Update with the final What's New campaign
      // value.
      return "XDeviceWhatsNew";
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      // TODO(crbug.com/579407047): Update with the final campaign value.
      return "XDeviceForeignTabs";
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      // TODO(crbug.com/564793759): Update with the final bookmark save campaign
      // value once registered.
      return "XDeviceBookmarkSave";
  }
}

GURL GetCrossDeviceSigninQrCodeUrl(CrossDeviceSigninPromoEntryPoint entry_point,
                                   const std::string& email) {
  // These query parameter names are used to track installs on iOS and Android.
  static constexpr char kCrossDevicePromoIOSCampaignQueryParam[] =
      "ios-campaign";
  static constexpr char kCrossDevicePromoAndroidCampaignQueryParam[] =
      "android-campaign";

  std::string_view campaign = GetCrossDevicePromoCampaign(entry_point);
  std::string base_url_str = switches::kCrossDeviceSigninFromDesktopUrl.Get();
  std::string url_str = base::ReplaceStringPlaceholders(
      base_url_str, {base::EscapeQueryParamValue(email, true)}, nullptr);
  GURL url(url_str);
  url = net::AppendOrReplaceQueryParameter(
      url, kCrossDevicePromoIOSCampaignQueryParam, campaign);
  url = net::AppendOrReplaceQueryParameter(
      url, kCrossDevicePromoAndroidCampaignQueryParam, campaign);
  return url;
}

// Key for the `QrBubbleState` stored on the `Profile`. It points to itself so
// that its address is unique and cannot be merged with another constant by
// identical code folding.
const void* const kQrBubbleStateUserDataKey = &kQrBubbleStateUserDataKey;

// Returns whether opening the QR code bubble from `entry_point` replaces a
// bubble that is already open for the profile (closing it and reopening it in
// the requesting window), rather than doing nothing.
bool ShouldReplaceExistingBubble(CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      // Permanent entry point, which cannot observe the bubble state to disable
      // itself: reopen the bubble where the user asked for it.
      return true;
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      // Promo cards disable their CTA while a bubble is open, so a click here
      // only races with the state update and must not steal the bubble.
      return false;
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      // Post-save promos must not steal an already open bubble.
      return false;
  }
  NOTREACHED();
}

// Tracks the "Sign in to phone" QR code bubble for a profile (at most one open
// across all browser windows).
class QrBubbleState : public base::SupportsUserData::Data {
 public:
  QrBubbleState() = default;
  QrBubbleState(const QrBubbleState&) = delete;
  QrBubbleState& operator=(const QrBubbleState&) = delete;
  ~QrBubbleState() override = default;

  static QrBubbleState* Get(Profile* profile) {
    if (QrBubbleState* existing = IfExists(profile)) {
      return existing;
    }
    auto new_state = std::make_unique<QrBubbleState>();
    QrBubbleState* state = new_state.get();
    profile->SetUserData(kQrBubbleStateUserDataKey, std::move(new_state));
    return state;
  }

  static QrBubbleState* IfExists(Profile* profile) {
    return static_cast<QrBubbleState*>(
        profile->GetUserData(kQrBubbleStateUserDataKey));
  }

  bool is_open() const { return active_bubble_weak_factory_.HasWeakPtrs(); }

  base::CallbackListSubscription AddObserver(
      base::RepeatingCallback<void(bool)> callback) {
    return observers_.Add(std::move(callback));
  }

  base::OnceClosure OnBubbleOpening(
      base::WeakPtr<SigninViewController> controller,
      base::OnceClosure closing_callback) {
    const bool was_open = is_open();
    // Disarm the replaced bubble's `OnBubbleClosed()` before closing its
    // widget.
    active_bubble_weak_factory_.InvalidateWeakPtrs();
    base::WeakPtr<QrBubbleState> weak_this =
        active_bubble_weak_factory_.GetWeakPtr();
    base::OnceClosure on_closed = base::BindOnce(
        [](base::WeakPtr<QrBubbleState> state, base::OnceClosure callback) {
          if (state) {
            state->OnBubbleClosed();
          }
          if (callback) {
            std::move(callback).Run();
          }
        },
        weak_this, std::move(closing_callback));

    if (auto previous_controller =
            std::exchange(open_controller_, std::move(controller))) {
      previous_controller->CloseBubbleSignin();
    }
    if (!was_open) {
      observers_.Notify(true);
    }
    return on_closed;
  }

 private:
  void OnBubbleClosed() {
    active_bubble_weak_factory_.InvalidateWeakPtrs();
    open_controller_ = nullptr;
    observers_.Notify(false);
  }

  base::WeakPtr<SigninViewController> open_controller_;
  base::RepeatingCallbackList<void(bool)> observers_;
  base::WeakPtrFactory<QrBubbleState> active_bubble_weak_factory_{this};
};

// Returns whether `entry_point` is enabled by its feature flags.
bool IsFeatureEnabledForEntryPoint(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop)) {
    return false;
  }
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return true;
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
      return base::FeatureList::IsEnabled(
                 switches::kCrossDeviceSigninFromDesktopFollowup) &&
             switches::kCrossDeviceSigninFromDesktopWhatsNew.Get();
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      return base::FeatureList::IsEnabled(
          switches::kCrossDeviceSigninFromDesktopFollowup);
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return base::FeatureList::IsEnabled(
                 switches::kCrossDeviceSigninFromDesktopFollowup) &&
             switches::kCrossDeviceSigninFromDesktopBookmarkSave.Get();
  }
  NOTREACHED();
}

// Returns whether the promo at `entry_point` is blocked when the account
// already has a phone.
bool ShouldCheckForMobileDevice(CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return true;
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      // Send Tab To Self is only triggered when there is no valid target
      // device, which already accounts for devices: a phone can be listed in
      // the device info without being a valid target (e.g. inactive), and the
      // promo must still be offered in that case.
      return false;
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
      // The What's New page always shows a QR code when the user asks for one,
      // so signed-in users get the account-bound one even if they already have
      // a phone (e.g. to set up a new one).
      return false;
  }
  NOTREACHED();
}

// Returns the sync selectable type required for `entry_point`, or std::nullopt
// for entry points that are not dependent on a specific data type.
std::optional<syncer::UserSelectableType> GetRequiredSyncTypeForEntryPoint(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return syncer::UserSelectableType::kHistory;
    case CrossDeviceSigninPromoEntryPoint::kForeignTabs:
      return syncer::UserSelectableType::kTabs;
    case CrossDeviceSigninPromoEntryPoint::kBookmarkSave:
      return syncer::UserSelectableType::kBookmarks;
    case CrossDeviceSigninPromoEntryPoint::kWhatsNewPage:
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return std::nullopt;
  }
  NOTREACHED();
}

// Checks the user's state for the promo at `entry_point`, ignoring the
// frequency caps: returns `kCanShow` if signed in with no error, without a
// synced mobile device (where applicable) and with the required data types
// enabled, or the reason it is not. Records no metrics.
CrossDeviceSigninPromoShouldShowResult CheckPromoPrerequisites(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  // Non-regular (incognito, guest, system) profiles can never see the promo.
  // Report them separately so they don't inflate `kNotSignedIn`.
  if (!profile->IsRegularProfile()) {
    return CrossDeviceSigninPromoShouldShowResult::kProfileTypeNotEligible;
  }
  // 1. General eligibility: Signed in with no errors, and (where applicable)
  // has NO mobile device.
  if (!IsUserSignedInWithNoError(profile)) {
    return CrossDeviceSigninPromoShouldShowResult::kNotSignedIn;
  }
  if (ShouldCheckForMobileDevice(entry_point) && HasMobileDevice(profile)) {
    return CrossDeviceSigninPromoShouldShowResult::kHasMobileDevice;
  }

  // 2. Data-type eligibility check. Entry points without a data-type
  // requirement return `std::nullopt` and are unaffected.
  if (std::optional<syncer::UserSelectableType> required_sync_type =
          GetRequiredSyncTypeForEntryPoint(entry_point);
      required_sync_type.has_value() &&
      !IsSyncTypeEnabled(profile, *required_sync_type)) {
    return CrossDeviceSigninPromoShouldShowResult::kDataTypeNotEnabled;
  }

  return CrossDeviceSigninPromoShouldShowResult::kCanShow;
}

bool OpenSigninToPhoneQrCodeBubbleInternal(
    BrowserWindowInterface* browser_window,
    CrossDeviceSigninPromoEntryPoint entry_point,
    base::OnceClosure promo_will_show_callback,
    base::OnceClosure closing_callback,
    base::OnceClosure explicit_dismissal_callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop));
  if (!browser_window) {
    return false;
  }
  Profile* profile = browser_window->GetProfile();
  if (!profile) {
    return false;
  }
  QrBubbleState* state = QrBubbleState::Get(profile);
  if (state->is_open() && !ShouldReplaceExistingBubble(entry_point)) {
    return false;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return false;
  }
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account_info.IsEmpty() || primary_account_info.email.empty()) {
    return false;
  }

  if (promo_will_show_callback) {
    std::move(promo_will_show_callback).Run();
  }
  base::UmaHistogramEnumeration(
      "Signin.CrossDeviceSigninPromo.OpenedQrCodeBubble", entry_point);
  GURL qr_code_url =
      GetCrossDeviceSigninQrCodeUrl(entry_point, primary_account_info.email);
  SigninViewController* controller = SigninViewController::From(browser_window);
  base::OnceClosure on_closed =
      state->OnBubbleOpening(controller ? controller->AsWeakPtr() : nullptr,
                             std::move(closing_callback));
  signin_ui_util::ShowCrossDeviceSigninQrBubble(
      browser_window, std::move(qr_code_url), std::move(on_closed), entry_point,
      std::move(explicit_dismissal_callback));
  return true;
}

}  // namespace

bool ShouldStartShowingCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  // The result is recorded even if the entry point is disabled, so that study
  // arms can be compared on the same trigger. Evaluating it has no side
  // effects.
  const bool is_entry_point_enabled =
      IsFeatureEnabledForEntryPoint(entry_point);
  if (!is_entry_point_enabled &&
      !base::FeatureList::IsEnabled(
          switches::kCrossDeviceSigninPromoEligibilityLogging)) {
    return false;
  }

  CrossDeviceSigninPromoShouldShowResult result =
      CheckPromoPrerequisites(entry_point, profile);
  if (result == CrossDeviceSigninPromoShouldShowResult::kCanShow) {
    // 3. Dismissible limit checking - only for dismissible entry points.
    if (std::optional<CrossDevicePromoSurface> surface =
            GetPromoSurfaceForEntryPoint(entry_point)) {
      result = CheckDismissiblePromoLimits(profile, *surface);
    }
  }

  RecordShouldShowResult(entry_point, result);
  return is_entry_point_enabled &&
         result == CrossDeviceSigninPromoShouldShowResult::kCanShow;
}

bool ShouldKeepShowingCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  return IsFeatureEnabledForEntryPoint(entry_point) &&
         CheckPromoPrerequisites(entry_point, profile) ==
             CrossDeviceSigninPromoShouldShowResult::kCanShow;
}

void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile) {
  std::optional<CrossDevicePromoState> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Shown tracking should not be called for "
                    "non-dismissible entry point";
  }

  data->shown_count = base::ClampAdd(data->shown_count, 1);
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.ShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  std::optional<CrossDevicePromoState> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Dismissal tracking should not be called for "
                    "non-dismissible entry point";
  }

  // Dismissals during the cooldown started on any surface count once, and do
  // not extend the cooldown. This can happen for promos shown before the
  // cooldown started, e.g. a History page promo card loaded in another tab.
  if (IsDismissiblePromoCooldownActive(SigninPrefs(*profile->GetPrefs()),
                                       GetPrimaryAccountGaiaId(profile))) {
    return;
  }
  data->dismissed_count = base::ClampAdd(data->dismissed_count, 1);
  data->last_dismissed_time = base::Time::Now();
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.DismissedAtShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

bool MaybeShowCrossDeviceSigninQrPromo(
    BrowserWindowInterface* browser_window,
    CrossDeviceSigninPromoEntryPoint entry_point,
    base::OnceClosure promo_will_show_callback,
    base::OnceClosure promo_closed_callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(GetPromoSurfaceForEntryPoint(entry_point).has_value());
  if (!browser_window) {
    return false;
  }
  Profile* profile = browser_window->GetProfile();
  if (!profile) {
    return false;
  }
  if (IsCrossDeviceSigninPromoBubbleOpen(profile) &&
      !ShouldReplaceExistingBubble(entry_point)) {
    return false;
  }
  if (!ShouldStartShowingCrossDeviceSigninPromo(entry_point, profile)) {
    return false;
  }

  base::OnceClosure will_show_and_record = base::BindOnce(
      [](base::OnceClosure promo_will_show_callback,
         CrossDeviceSigninPromoEntryPoint entry_point, Profile* profile) {
        if (promo_will_show_callback) {
          std::move(promo_will_show_callback).Run();
        }
        OnCrossDeviceSigninPromoShown(entry_point, profile);
      },
      std::move(promo_will_show_callback), entry_point, profile);

  // The closing callback runs once when this bubble closes for any reason,
  // including its replacement by another QR code bubble and failing to create
  // it, so `promo_closed_callback` runs at most once too.
  // `explicit_dismissal_callback` only runs when the user explicitly closes
  // the bubble (close button or Escape key).
  return OpenSigninToPhoneQrCodeBubbleInternal(
      browser_window, entry_point, std::move(will_show_and_record),
      std::move(promo_closed_callback),
      base::BindOnce(
          [](base::WeakPtr<Profile> weak_profile,
             CrossDeviceSigninPromoEntryPoint entry_point) {
            if (weak_profile && ShouldKeepShowingCrossDeviceSigninPromo(
                                    entry_point, weak_profile.get())) {
              OnCrossDeviceSigninPromoDismissed(entry_point,
                                                weak_profile.get());
            }
          },
          profile->GetWeakPtr(), entry_point));
}

void OpenSigninToPhoneQrCodeBubble(BrowserWindowInterface* browser_window,
                                   CrossDeviceSigninPromoEntryPoint entry_point,
                                   base::OnceClosure closing_callback) {
  OpenSigninToPhoneQrCodeBubbleInternal(
      browser_window, entry_point,
      /*promo_will_show_callback=*/base::OnceClosure(),
      std::move(closing_callback),
      /*explicit_dismissal_callback=*/base::OnceClosure());
}

bool IsCrossDeviceSigninPromoBubbleOpen(Profile* profile) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(profile);
  QrBubbleState* state = QrBubbleState::IfExists(profile);
  return state && state->is_open();
}

base::CallbackListSubscription
RegisterCrossDeviceSigninPromoBubbleStateCallback(
    Profile* profile,
    base::RepeatingCallback<void(bool)> callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI, base::NotFatalUntil::M162);
  CHECK(profile);
  return QrBubbleState::Get(profile)->AddObserver(std::move(callback));
}

std::string_view GetEntryPointHistogramSuffixForTesting(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  return GetEntryPointHistogramSuffix(entry_point);
}
