// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notifier_service.h"

#include <algorithm>

#include "base/android/application_status_listener.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/android/ntp/content_suggestions_notifier.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_metrics.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ntp_snippets/features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::KnownCategories;
using ntp_snippets::kNotificationsDailyLimit;
using ntp_snippets::kNotificationsDefaultDailyLimit;
using ntp_snippets::kNotificationsDefaultPriority;
using ntp_snippets::kNotificationsFeature;
using ntp_snippets::kNotificationsKeepWhenFrontmostParam;
using ntp_snippets::kNotificationsOpenToNTPParam;
using ntp_snippets::kNotificationsPriorityParam;
using ntp_snippets::kNotificationsTextParam;
using ntp_snippets::kNotificationsTextValueAndMore;
using ntp_snippets::kNotificationsTextValueSnippet;

base::android::ApplicationState*
    g_content_suggestions_notification_application_state_for_testing = nullptr;

namespace {

const char kNotificationIDWithinCategory[] =
    "ContentSuggestionsNotificationIDWithinCategory";

gfx::Image CropSquare(const gfx::Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
  const gfx::ImageSkia* skimage = image.ToImageSkia();
  gfx::Rect bounds{{0, 0}, skimage->size()};
  int size = std::min(bounds.width(), bounds.height());
  bounds.ClampToCenteredSize({size, size});
  return gfx::Image(gfx::ImageSkiaOperations::CreateTiledImage(
      *skimage, bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

bool ShouldNotifyInState(base::android::ApplicationState state) {
  if (g_content_suggestions_notification_application_state_for_testing) {
    state = *g_content_suggestions_notification_application_state_for_testing;
  }
  switch (state) {
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return false;
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return true;
  }
  NOTREACHED();
  return false;
}

int DayAsYYYYMMDD() {
  base::Time::Exploded now{};
  base::Time::Now().LocalExplode(&now);
  return (now.year * 10000) + (now.month * 100) + now.day_of_month;
}

bool HaveQuotaForToday(PrefService* prefs) {
  int today = DayAsYYYYMMDD();
  int limit = variations::GetVariationParamByFeatureAsInt(
      kNotificationsFeature, kNotificationsDailyLimit,
      kNotificationsDefaultDailyLimit);
  int sent =
      prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentDay) == today
          ? prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentCount)
          : 0;
  return sent < limit;
}

void ConsumeQuota(PrefService* prefs) {
  int sent =
      prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentCount);
  int today = DayAsYYYYMMDD();
  if (prefs->GetInteger(prefs::kContentSuggestionsNotificationsSentDay) !=
      today) {
    prefs->SetInteger(prefs::kContentSuggestionsNotificationsSentDay, today);
    sent = 0;  // Reset on day change.
  }
  prefs->SetInteger(prefs::kContentSuggestionsNotificationsSentCount, sent + 1);
}

}  // namespace

class ContentSuggestionsNotifierService::NotifyingObserver
    : public ContentSuggestionsService::Observer {
 public:
  NotifyingObserver(ContentSuggestionsService* service,
                    PrefService* prefs,
                    ContentSuggestionsNotifier* notifier)
      : service_(service),
        prefs_(prefs),
        notifier_(notifier),
        app_status_listener_(base::android::ApplicationStatusListener::New(
            base::BindRepeating(&NotifyingObserver::AppStatusChanged,
                                base::Unretained(this)))),
        weak_ptr_factory_(this) {}

  void OnNewSuggestions(Category category) override {
    if (!ShouldNotifyInState(app_status_listener_->GetState())) {
      DVLOG(1) << "Suppressed notification because Chrome is frontmost";
      return;
    } else if (!ContentSuggestionsNotifier::ShouldSendNotifications(prefs_)) {
      DVLOG(1) << "Suppressed notification due to opt-out";
      return;
    } else if (!HaveQuotaForToday(prefs_)) {
      DVLOG(1) << "Notification suppressed due to daily limit";
      return;
    }
    const ContentSuggestion* suggestion = GetSuggestionToNotifyAbout(category);
    if (!suggestion) {
      return;
    }
    base::Time timeout_at = suggestion->notification_extra()
                                ? suggestion->notification_extra()->deadline
                                : base::Time::Max();

    const std::string text_param = variations::GetVariationParamValueByFeature(
        kNotificationsFeature, kNotificationsTextParam);
    base::string16 text;
    if (text_param == kNotificationsTextValueSnippet) {
      text = suggestion->snippet_text();
    } else if (text_param == kNotificationsTextValueAndMore) {
      int extra_count =
          service_->GetSuggestionsForCategory(category).size() - 1;
      text = l10n_util::GetStringFUTF16(
          IDS_NTP_NOTIFICATIONS_READ_THIS_STORY_AND_MORE,
          suggestion->publisher_name(), base::IntToString16(extra_count));
    } else {
      text = suggestion->publisher_name();
    }

    bool open_to_ntp = variations::GetVariationParamByFeatureAsBool(
        kNotificationsFeature, kNotificationsOpenToNTPParam, false);
    service_->FetchSuggestionImage(
        suggestion->id(),
        base::Bind(
            &NotifyingObserver::ImageFetched, weak_ptr_factory_.GetWeakPtr(),
            suggestion->id(),
            open_to_ntp ? GURL(chrome::kChromeUINewTabURL) : suggestion->url(),
            suggestion->title(), text, timeout_at));
  }

  void OnCategoryStatusChanged(Category category,
                               CategoryStatus new_status) override {
    if (!category.IsKnownCategory(KnownCategories::ARTICLES)) {
      return;
    }
    if (!ntp_snippets::IsCategoryStatusAvailable(new_status)) {
      notifier_->HideAllNotifications(
          ContentSuggestionsNotificationAction::HIDE_DISABLED);
    }
  }

  void OnSuggestionInvalidated(
      const ContentSuggestion::ID& suggestion_id) override {
    notifier_->HideNotification(
        suggestion_id, ContentSuggestionsNotificationAction::HIDE_EXPIRY);
  }

  void OnFullRefreshRequired() override {
    notifier_->HideAllNotifications(
        ContentSuggestionsNotificationAction::HIDE_EXPIRY);
  }

  void ContentSuggestionsServiceShutdown() override {
    notifier_->HideAllNotifications(
        ContentSuggestionsNotificationAction::HIDE_SHUTDOWN);
  }

 private:
  const ContentSuggestion* GetSuggestionToNotifyAbout(Category category) {
    const auto& suggestions = service_->GetSuggestionsForCategory(category);
    for (const ContentSuggestion& suggestion : suggestions) {
      if (suggestion.notification_extra()) {
        return &suggestion;
      }
    }
    return nullptr;
  }

  void AppStatusChanged(base::android::ApplicationState state) {
    if (variations::GetVariationParamByFeatureAsBool(
            kNotificationsFeature, kNotificationsKeepWhenFrontmostParam,
            true)) {
      return;
    }
    if (!ShouldNotifyInState(state)) {
      notifier_->HideAllNotifications(
          ContentSuggestionsNotificationAction::HIDE_FRONTMOST);
    }
  }

  void ImageFetched(const ContentSuggestion::ID& id,
                    const GURL& url,
                    const base::string16& title,
                    const base::string16& text,
                    base::Time timeout_at,
                    const gfx::Image& image) {
    if (!ShouldNotifyInState(app_status_listener_->GetState())) {
      return;  // Became foreground while we were fetching the image; forget it.
    }
    // check if suggestion is still valid.
    DVLOG(1) << "Fetched " << image.Size().width() << "x"
             << image.Size().height() << " image for " << url.spec();
    ConsumeQuota(prefs_);
    int priority = variations::GetVariationParamByFeatureAsInt(
        kNotificationsFeature, kNotificationsPriorityParam,
        kNotificationsDefaultPriority);
    if (notifier_->SendNotification(id, url, title, text, CropSquare(image),
                                    timeout_at, priority)) {
      RecordContentSuggestionsNotificationImpression(
          id.category().IsKnownCategory(KnownCategories::ARTICLES)
              ? ContentSuggestionsNotificationImpression::ARTICLE
              : ContentSuggestionsNotificationImpression::NONARTICLE);
    }
  }

  ContentSuggestionsService* const service_;
  PrefService* const prefs_;
  ContentSuggestionsNotifier* const notifier_;
  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;

  base::WeakPtrFactory<NotifyingObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingObserver);
};

ContentSuggestionsNotifierService::ContentSuggestionsNotifierService(
    PrefService* prefs,
    ContentSuggestionsService* suggestions,
    std::unique_ptr<ContentSuggestionsNotifier> notifier)
    : prefs_(prefs),
      suggestions_service_(suggestions),
      notifier_(std::move(notifier)) {
  notifier_->FlushCachedMetrics();

  if (notifier_->RegisterChannel(
          prefs_->GetBoolean(prefs::kContentSuggestionsNotificationsEnabled))) {
    // Once there is a notification channel, this setting is no longer relevant
    // and the UI to control it is gone. Set it to true so that notifications
    // are sent unconditionally. If the channel is disabled, then notifications
    // will be blocked at the system level, and the user can re-enable them in
    // the system notification settings.
    prefs_->SetBoolean(prefs::kContentSuggestionsNotificationsEnabled, true);
  }

  if (IsEnabled()) {
    Enable();
  } else {
    Disable();
  }
}

ContentSuggestionsNotifierService::~ContentSuggestionsNotifierService() =
    default;

void ContentSuggestionsNotifierService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kContentSuggestionsNotificationsEnabled,
                                true);
  registry->RegisterIntegerPref(
      prefs::kContentSuggestionsConsecutiveIgnoredPrefName, 0);
  registry->RegisterIntegerPref(prefs::kContentSuggestionsNotificationsSentDay,
                                0);
  registry->RegisterIntegerPref(
      prefs::kContentSuggestionsNotificationsSentCount, 0);

  // TODO(sfiera): remove after M62; no longer (and never really) used.
  registry->RegisterStringPref(kNotificationIDWithinCategory, std::string());
}

void ContentSuggestionsNotifierService::SetEnabled(bool enabled) {
  prefs_->SetBoolean(prefs::kContentSuggestionsNotificationsEnabled, enabled);
  if (enabled) {
    Enable();
  } else {
    Disable();
    RecordContentSuggestionsNotificationOptOut(
        ContentSuggestionsNotificationOptOut::EXPLICIT);
  }
}

bool ContentSuggestionsNotifierService::IsEnabled() const {
  return prefs_->GetBoolean(prefs::kContentSuggestionsNotificationsEnabled);
}

void ContentSuggestionsNotifierService::Enable() {
  if (!observer_) {
    observer_.reset(
        new NotifyingObserver(suggestions_service_, prefs_, notifier_.get()));
    suggestions_service_->AddObserver(observer_.get());
  }
}

void ContentSuggestionsNotifierService::Disable() {
  if (observer_) {
    suggestions_service_->RemoveObserver(observer_.get());
    observer_.reset();
  }
}
