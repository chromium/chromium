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

// These query parameter names are used to track installs on iOS and Android.
constexpr std::string_view kCrossDevicePromoIOSCampaignQueryParam =
    "ios-campaign";
constexpr std::string_view kCrossDevicePromoAndroidCampaignQueryParam =
    "android-campaign";

// Campaign values for cross-device sign-in QR code bubble entry points.
constexpr std::string_view kCrossDeviceProfileMenuCampaign =
    "XDeviceProfileMenu";
constexpr std::string_view kCrossDeviceHistoryPageCampaign =
    "XDeviceHistoryPage";
constexpr std::string_view kCrossDeviceSendTabToSelfCampaign =
    "XDeviceSendTabToSelf";

// Limits for the dismissible promo.
constexpr int kMaxShownCount = 5;
constexpr base::TimeDelta kDismissiblePromoCooldownPeriod = base::Days(7);

struct CrossDeviceSigninPromoData {
  int shown_count = 0;
  base::Time last_dismissed_time;
  bool shown_after_dismissal = false;
};

std::string_view GetEntryPointHistogramSuffix(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  // LINT.IfChange(CrossDeviceSigninPromoEntryPointVariant)
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return "HistoryPage";
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return "ProfileMenu";
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return "SendTabToSelf";
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

// Returns empty/defaulted data if the sub-dictionary is not set yet.
CrossDeviceSigninPromoData ReadDismissiblePromoData(
    Profile* profile,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  GaiaId gaia_id = GetPrimaryAccountGaiaId(profile);
  SigninPrefs signin_prefs(*profile->GetPrefs());
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return {
          .shown_count =
              signin_prefs.GetCrossDeviceHistoryPromoShownCount(gaia_id),
          .last_dismissed_time =
              signin_prefs.GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                  .value_or(base::Time()),
          .shown_after_dismissal =
              signin_prefs.GetCrossDeviceHistoryPromoShownAfterDismissal(
                  gaia_id),
      };
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      NOTREACHED() << "Entry point does not have any promo data";
  }
}

void WriteDismissiblePromoData(Profile* profile,
                               CrossDeviceSigninPromoEntryPoint entry_point,
                               const CrossDeviceSigninPromoData& data) {
  GaiaId gaia_id = GetPrimaryAccountGaiaId(profile);
  SigninPrefs signin_prefs(*profile->GetPrefs());
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      signin_prefs.SetCrossDeviceHistoryPromoShownCount(gaia_id,
                                                        data.shown_count);
      signin_prefs.SetCrossDeviceHistoryPromoShownAfterDismissal(
          gaia_id, data.shown_after_dismissal);
      if (!data.last_dismissed_time.is_null()) {
        signin_prefs.SetCrossDeviceHistoryPromoLastDismissedTime(
            gaia_id, data.last_dismissed_time);
      } else {
        signin_prefs.ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
      }
      return;
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      NOTREACHED() << "Entry point does not have any promo data";
  }
}

// Returns std::nullopt for non-dismissible promo entry points.
std::optional<CrossDeviceSigninPromoData> GetPromoDataForDismissibleEntryPoint(
    Profile* profile,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return ReadDismissiblePromoData(profile, entry_point);
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return std::nullopt;
  }
}

// Limits for the dismissible promo:
// - Promo can be shown at most 5 times.
// - Promo can be shown at most once after dismissal.
// - Cooldown period of 7 days after dismissal.
bool IsDismissiblePromoLimitReached(
    const CrossDeviceSigninPromoData& data,
    CrossDeviceSigninPromoEntryPoint entry_point) {
  if (data.shown_count >= kMaxShownCount) {
    RecordShouldShowResult(
        entry_point,
        CrossDeviceSigninPromoShouldShowResult::kShownLimitReached);
    return true;
  }
  if (data.shown_after_dismissal) {
    RecordShouldShowResult(entry_point,
                           CrossDeviceSigninPromoShouldShowResult::
                               kAlreadyShownAfterDismissalLimitReached);
    return true;
  }
  if (!data.last_dismissed_time.is_null()) {
    if (base::Time::Now() <
        data.last_dismissed_time + kDismissiblePromoCooldownPeriod) {
      RecordShouldShowResult(
          entry_point, CrossDeviceSigninPromoShouldShowResult::kCooldownActive);
      return true;
    }
  }
  return false;
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

bool IsHistorySyncEnabled(Profile* profile) {
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  if (!sync_service) {
    return false;
  }
  return sync_service->GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kHistory);
}

std::string_view GetCrossDevicePromoCampaign(
    CrossDeviceSigninPromoEntryPoint entry_point) {
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
      return kCrossDeviceProfileMenuCampaign;
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      return kCrossDeviceHistoryPageCampaign;
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      return kCrossDeviceSendTabToSelfCampaign;
  }
}

GURL GetCrossDeviceSigninQrCodeUrl(CrossDeviceSigninPromoEntryPoint entry_point,
                                   const std::string& email) {
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
      // Permanent entry point, which cannot observe the bubble state to disable
      // itself: reopen the bubble where the user asked for it.
      return true;
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      // Promo cards disable their CTA while a bubble is open, so a click here
      // only races with the state update and must not steal the bubble.
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
    base::OnceClosure on_closed =
        base::BindOnce(&QrBubbleState::OnBubbleClosed,
                       active_bubble_weak_factory_.GetWeakPtr());
    if (closing_callback) {
      on_closed = std::move(on_closed).Then(std::move(closing_callback));
    }

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

}  // namespace

bool ShouldShowCrossDeviceSigninPromo(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  if (!base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop)) {
    return false;
  }

  // 1. General eligibility: Signed in with no errors, and has NO other devices.
  if (!IsUserSignedInWithNoError(profile)) {
    RecordShouldShowResult(
        entry_point, CrossDeviceSigninPromoShouldShowResult::kNotSignedIn);
    return false;
  }
  if (HasMobileDevice(profile)) {
    RecordShouldShowResult(
        entry_point, CrossDeviceSigninPromoShouldShowResult::kHasMobileDevice);
    return false;
  }

  // 2. Data-type eligibility check.
  switch (entry_point) {
    case CrossDeviceSigninPromoEntryPoint::kHistoryPage:
      if (!IsHistorySyncEnabled(profile)) {
        RecordShouldShowResult(
            entry_point,
            CrossDeviceSigninPromoShouldShowResult::kDataTypeNotEnabled);
        return false;
      }
      break;
    case CrossDeviceSigninPromoEntryPoint::kProfileMenu:
    case CrossDeviceSigninPromoEntryPoint::kSendTabToSelf:
      // Permanent entry point, no data-type constraints.
      break;
  }

  // 3. Dismissible limit checking - only for dismissible entry points.
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (data.has_value() && IsDismissiblePromoLimitReached(*data, entry_point)) {
    return false;
  }

  RecordShouldShowResult(entry_point,
                         CrossDeviceSigninPromoShouldShowResult::kCanShow);
  return true;
}

void OnCrossDeviceSigninPromoShown(CrossDeviceSigninPromoEntryPoint entry_point,
                                   Profile* profile) {
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Shown tracking should not be called for "
                    "non-dismissible entry point";
  }

  data->shown_count = base::ClampAdd(data->shown_count, 1);
  if (!data->last_dismissed_time.is_null()) {
    data->shown_after_dismissal = true;
  }
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.ShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

void OnCrossDeviceSigninPromoDismissed(
    CrossDeviceSigninPromoEntryPoint entry_point,
    Profile* profile) {
  std::optional<CrossDeviceSigninPromoData> data =
      GetPromoDataForDismissibleEntryPoint(profile, entry_point);
  if (!data.has_value()) {
    NOTREACHED() << "Dismissal tracking should not be called for "
                    "non-dismissible entry point";
  }

  data->last_dismissed_time = base::Time::Now();
  WriteDismissiblePromoData(profile, entry_point, *data);

  base::UmaHistogramExactLinear(
      base::StrCat({"Signin.CrossDeviceSigninPromo.DismissedAtShownCount.",
                    GetEntryPointHistogramSuffix(entry_point)}),
      data->shown_count, kMaxShownCount + 1);
}

void OpenSigninToPhoneQrCodeBubble(BrowserWindowInterface* browser_window,
                                   CrossDeviceSigninPromoEntryPoint entry_point,
                                   base::OnceClosure closing_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(base::FeatureList::IsEnabled(switches::kCrossDeviceSigninFromDesktop));
  if (!browser_window) {
    return;
  }
  Profile* profile = browser_window->GetProfile();
  if (!profile) {
    return;
  }
  QrBubbleState* state = QrBubbleState::Get(profile);
  if (state->is_open() && !ShouldReplaceExistingBubble(entry_point)) {
    return;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return;
  }
  CoreAccountInfo primary_account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (primary_account_info.IsEmpty() || primary_account_info.email.empty()) {
    return;
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
      browser_window, std::move(qr_code_url), std::move(on_closed),
      entry_point);
}

bool IsCrossDeviceSigninPromoBubbleOpen(Profile* profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);
  QrBubbleState* state = QrBubbleState::IfExists(profile);
  return state && state->is_open();
}

base::CallbackListSubscription
RegisterCrossDeviceSigninPromoBubbleStateCallback(
    Profile* profile,
    base::RepeatingCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(profile);
  return QrBubbleState::Get(profile)->AddObserver(std::move(callback));
}
