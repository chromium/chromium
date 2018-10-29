// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/android/feed/feed_host_service_factory.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/strike_database_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/navigation_entry_remover.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_blacklist.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service.h"
#include "chrome/browser/data_use_measurement/page_load_capping/page_load_capping_service_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/language/url_language_histogram_factory.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/webrtc/webrtc_event_log_manager.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/strike_database.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/browsing_data/core/features.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/device_event_log/device_event_log.h"
#include "components/domain_reliability/service.h"
#include "components/feed/content/feed_host_service.h"
#include "components/feed/core/feed_scheduler_host.h"
#include "components/history/core/browser/history_service.h"
#include "components/language/core/browser/url_language_histogram.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_ui_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "components/webrtc_logging/browser/log_cleanup.h"
#include "components/webrtc_logging/browser/text_log_list.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/plugin_data_remover.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/storage_partition.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/http/http_transaction_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/customtabs/origin_verifier.h"
#include "chrome/browser/android/feed/feed_lifecycle_bridge.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/search_permissions/search_permissions_service.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "chrome/browser/media/android/cdm/media_drm_license_manager.h"
#include "chrome/browser/offline_pages/offline_page_model_factory.h"
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "sql/database.h"
#endif  // defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/constants.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/attestation_constants.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MACOSX)
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "device/fido/mac/credential_store.h"
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;
using content::BrowsingDataFilterBuilder;

namespace {

// Generic functions but currently only used when ENABLE_NACL.
#if BUILDFLAG(ENABLE_NACL)
void UIThreadTrampolineHelper(base::OnceClosure callback) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI}, std::move(callback));
}

// Convenience method to create a callback that can be run on any thread and
// will post the given |callback| back to the UI thread.
base::OnceClosure UIThreadTrampoline(base::OnceClosure callback) {
  // We could directly bind &base::PostTaskWithTraits, but that would require
  // evaluating FROM_HERE when this method is called, as opposed to when the
  // task is actually posted.
  return base::BindOnce(&UIThreadTrampolineHelper, std::move(callback));
}
#endif  // BUILDFLAG(ENABLE_NACL)

template <typename T>
void IgnoreArgumentHelper(base::OnceClosure callback, T unused_argument) {
  std::move(callback).Run();
}

// Another convenience method to turn a callback without arguments into one that
// accepts (and ignores) a single argument.
template <typename T>
base::OnceCallback<void(T)> IgnoreArgument(base::OnceClosure callback) {
  return base::BindOnce(&IgnoreArgumentHelper<T>, std::move(callback));
}

bool WebsiteSettingsFilterAdapter(
    const base::RepeatingCallback<bool(const GURL&)> predicate,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  // Ignore the default setting.
  if (primary_pattern == ContentSettingsPattern::Wildcard())
    return false;

  // Website settings only use origin-scoped patterns. The only content setting
  // currently supported by ChromeBrowsingDataRemoverDelegate is
  // DURABLE_STORAGE, which also only uses origin-scoped patterns. Such patterns
  // can be directly translated to a GURL.
  GURL url(primary_pattern.ToString());
  DCHECK(url.is_valid());
  return predicate.Run(url);
}

#if BUILDFLAG(ENABLE_NACL)
void ClearNaClCacheOnIOThread(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  nacl::NaClBrowser::GetInstance()->ClearValidationCache(callback);
}

void ClearPnaclCacheOnIOThread(base::Time begin,
                               base::Time end,
                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  pnacl::PnaclHost::GetInstance()->ClearTranslationCacheEntriesBetween(
      begin, end, callback);
}
#endif

#if defined(OS_ANDROID)
void ClearPrecacheInBackground(content::BrowserContext* browser_context) {
  // Precache code was removed in M61 but the sqlite database file could be
  // still here.
  base::FilePath db_path(browser_context->GetPath().Append(
      base::FilePath(FILE_PATH_LITERAL("PrecacheDatabase"))));
  sql::Database::Delete(db_path);
}
#endif

// Returned by ChromeBrowsingDataRemoverDelegate::GetOriginTypeMatcher().
bool DoesOriginMatchEmbedderMask(int origin_type_mask,
                                 const GURL& origin,
                                 storage::SpecialStoragePolicy* policy) {
  DCHECK_EQ(
      0,
      origin_type_mask &
          (ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EMBEDDER_BEGIN - 1))
      << "|origin_type_mask| can only contain origin types defined in "
      << "the embedder.";

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Packaged apps and extensions match iff EXTENSION.
  if ((origin.GetOrigin().scheme() == extensions::kExtensionScheme) &&
      (origin_type_mask &
       ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION)) {
    return true;
  }
  origin_type_mask &= ~ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION;
#endif

  DCHECK(!origin_type_mask)
      << "DoesOriginMatchEmbedderMask must handle all origin types.";

  return false;
}

// Callback for when cookies have been deleted. Invokes NotifyIfDone.
// Receiving |cookie_manager| as a parameter so that the receive pipe is
// not deleted before the response is received.
void OnClearedCookies(base::OnceClosure done,
                      network::mojom::CookieManagerPtr cookie_manager,
                      uint32_t num_deleted) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(done).Run();
}

}  // namespace

ChromeBrowsingDataRemoverDelegate::ChromeBrowsingDataRemoverDelegate(
    BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)),
#if BUILDFLAG(ENABLE_PLUGINS)
      flash_lso_helper_(BrowsingDataFlashLSOHelper::Create(browser_context)),
#endif
#if defined(OS_ANDROID)
      webapp_registry_(new WebappRegistry()),
#endif
      weak_ptr_factory_(this) {
}

ChromeBrowsingDataRemoverDelegate::~ChromeBrowsingDataRemoverDelegate() {}

void ChromeBrowsingDataRemoverDelegate::Shutdown() {
  history_task_tracker_.TryCancelAll();
  template_url_sub_.reset();
}

content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
ChromeBrowsingDataRemoverDelegate::GetOriginTypeMatcher() const {
  return base::BindRepeating(&DoesOriginMatchEmbedderMask);
}


bool ChromeBrowsingDataRemoverDelegate::MayRemoveDownloadHistory() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory);
}

void ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    const BrowsingDataFilterBuilder& filter_builder,
    int origin_type_mask,
    base::OnceClosure callback) {
  DCHECK(((remove_mask &
           ~content::BrowsingDataRemover::DATA_TYPE_AVOID_CLOSING_CONNECTIONS &
           ~FILTERABLE_DATA_TYPES) == 0) ||
         filter_builder.IsEmptyBlacklist());

  // Embedder-defined DOM-accessible storage currently contains only
  // one datatype, which is the durable storage permission.
  if (remove_mask &
      content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE) {
    remove_mask |= DATA_TYPE_DURABLE_PERMISSION;
  }

  if (origin_type_mask &
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsUnprotectedWeb"));
  }
  if (origin_type_mask &
      content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsProtectedWeb"));
  }
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin_type_mask & ORIGIN_TYPE_EXTENSION) {
    base::RecordAction(
        UserMetricsAction("ClearBrowsingData_MaskContainsExtension"));
  }
#endif
  // If this fires, we added a new BrowsingDataHelper::OriginTypeMask without
  // updating the user metrics above.
  static_assert(
      ALL_ORIGIN_TYPES ==
          (content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
#if BUILDFLAG(ENABLE_EXTENSIONS)
           ORIGIN_TYPE_EXTENSION |
#endif
           content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB),
      "OriginTypeMask has been updated without updating user metrics");

  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  base::ScopedClosureRunner synchronous_clear_operations(
      CreatePendingTaskCompletionClosure());
  callback_ = std::move(callback);

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;

  base::RepeatingCallback<bool(const GURL& url)> filter =
      filter_builder.BuildGeneralFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::RepeatingCallback<bool(const GURL&)> nullable_filter =
      filter_builder.IsEmptyBlacklist()
          ? base::RepeatingCallback<bool(const GURL&)>()
          : filter;

  // Managed devices and supervised users can have restrictions on history
  // deletion.
  PrefService* prefs = profile_->GetPrefs();
  bool may_delete_history = prefs->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);

  // All the UI entry points into the BrowsingDataRemoverImpl should be
  // disabled, but this will fire if something was missed or added.
  DCHECK(may_delete_history ||
         (remove_mask & content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS) ||
         (!(remove_mask & DATA_TYPE_HISTORY) &&
          !(remove_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS)));

  HostContentSettingsMap* host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_HISTORY
  if ((remove_mask & DATA_TYPE_HISTORY) && may_delete_history) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      base::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      history_service->ExpireLocalAndRemoteHistoryBetween(
          WebHistoryServiceFactory::GetForProfile(profile_), std::set<GURL>(),
          delete_begin_, delete_end_,
          base::AdaptCallbackForRepeating(CreatePendingTaskCompletionClosure()),
          &history_task_tracker_);
    }
    if (ClipboardRecentContent::GetInstance())
      ClipboardRecentContent::GetInstance()->SuppressClipboardContent();

    // Currently, ContentSuggestionService instance exists only on Android.
    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service) {
      content_suggestions_service->ClearHistory(delete_begin_, delete_end_,
                                                filter);
    }

    // Remove the last visit dates meta-data from the bookmark model.
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile_);
    if (bookmark_model) {
      ntp_snippets::RemoveLastVisitedDatesBetween(delete_begin_, delete_end_,
                                                  filter, bookmark_model);
    }

#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
    feed::FeedHostService* feed_host_service =
        feed::FeedHostServiceFactory::GetForBrowserContext(profile_);
    if (feed_host_service) {
      feed_host_service->GetSchedulerHost()->OnHistoryCleared();
    }
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#endif  // defined(OS_ANDROID)

    language::UrlLanguageHistogram* language_histogram =
        UrlLanguageHistogramFactory::GetForBrowserContext(profile_);
    if (language_histogram) {
      language_histogram->ClearHistory(delete_begin_, delete_end_);
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The extension activity log contains details of which websites extensions
    // were active on. It therefore indirectly stores details of websites a
    // user has visited so best clean from here as well.
    // TODO(msramek): Support all backends with filter (crbug.com/589586).
    extensions::ActivityLog::GetInstance(profile_)->RemoveURLs(
        std::set<GURL>());

    // Clear launch times as they are a form of history.
    // BrowsingDataFilterBuilder currently doesn't support extension origins.
    // Therefore, clearing history for a small set of origins (WHITELIST) should
    // never delete any extension launch times, while clearing for almost all
    // origins (BLACKLIST) should always delete all of extension launch times.
    if (filter_builder.IsEmptyBlacklist()) {
      extensions::ExtensionPrefs* extension_prefs =
          extensions::ExtensionPrefs::Get(profile_);
      extension_prefs->ClearLastLaunchTimes();
    }
#endif

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history. We have no mechanism to track when these items were
    // created, so we'll not honor the time range.
    BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetNetworkContext()
        ->ClearHostCache(filter_builder.BuildNetworkServiceFilter(),
                         CreatePendingTaskCompletionClosureForMojo());

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLService* keywords_model =
        TemplateURLServiceFactory::GetForProfile(profile_);

    if (keywords_model && !keywords_model->loaded()) {
      // TODO(msramek): Store filters from the currently executed task on the
      // object to avoid having to copy them to callback methods.
      template_url_sub_ = keywords_model->RegisterOnLoadedCallback(
          base::AdaptCallbackForRepeating(base::BindOnce(
              &ChromeBrowsingDataRemoverDelegate::OnKeywordsLoaded,
              weak_ptr_factory_.GetWeakPtr(), filter,
              CreatePendingTaskCompletionClosure())));
      keywords_model->Load();
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedForUrlsBetween(filter, delete_begin_,
                                                        delete_end_);
    }

    // The PrerenderManager keeps history of prerendered pages, so clear that.
    // It also may have a prerendered page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS |
          prerender::PrerenderManager::CLEAR_PRERENDER_HISTORY);
    }

    // When this feature is enabled, recent tabs and sessions will be deleted
    // by NavigationEntryRemover and not here.
    bool is_navigation_entry_remover_enabled = base::FeatureList::IsEnabled(
        browsing_data::features::kRemoveNavigationHistory);

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    if (!is_navigation_entry_remover_enabled &&
        filter_builder.GetMode() == BrowsingDataFilterBuilder::BLACKLIST) {
      // We also delete the list of recently closed tabs. Since these expire,
      // they can't be more than a day old, so we can simply clear them all.
      sessions::TabRestoreService* tab_service =
          TabRestoreServiceFactory::GetForProfile(profile_);
      if (tab_service) {
        tab_service->ClearEntries();
        tab_service->DeleteLastSession();
      }

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
      // We also delete the last session when we delete the history.
      SessionService* session_service =
          SessionServiceFactory::GetForProfile(profile_);
      if (session_service)
        session_service->DeleteLastSession();
#endif
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    // TODO(dmurph): Support all backends with filter (crbug.com/113621).
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      web_data_service->RemoveOriginURLsModifiedBetween(
          delete_begin_, delete_end_);
      // Ask for a call back when the above call is finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), CreatePendingTaskCompletionClosure());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }

    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            &webrtc_logging::DeleteOldAndRecentWebRtcLogFiles,
            webrtc_logging::TextLogList::
                GetWebRtcLogDirectoryForBrowserContextPath(profile_->GetPath()),
            delete_begin_),
        CreatePendingTaskCompletionClosure());

#if defined(OS_ANDROID)
    base::PostTaskWithTraitsAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&ClearPrecacheInBackground, profile_),
        CreatePendingTaskCompletionClosure());

    // Clear the history information (last launch time and origin URL) of any
    // registered webapps.
    webapp_registry_->ClearWebappHistoryForUrls(filter);

    // The OriginVerifier caches origins for Trusted Web Activities that have
    // been verified and stores them in Android Preferences.
    customtabs::OriginVerifier::ClearBrowsingData();
#endif

    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                profile_);
    // |data_reduction_proxy_settings| is null if |profile_| is off the record.
    if (data_reduction_proxy_settings) {
      data_reduction_proxy::DataReductionProxyService*
          data_reduction_proxy_service =
              data_reduction_proxy_settings->data_reduction_proxy_service();
      if (data_reduction_proxy_service) {
        data_reduction_proxy_service->compression_stats()
            ->DeleteBrowsingHistory(delete_begin_, delete_end_);
      }
    }

    // |previews_service| is null if |profile_| is off the record.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(profile_);
    if (previews_service)
      previews_service->ClearBlackList(delete_begin_, delete_end_);

    // |previews_service| is null if |profile_| is off the record.
    PageLoadCappingService* page_load_capping_service =
        PageLoadCappingServiceFactory::GetForBrowserContext(profile_);
    if (page_load_capping_service &&
        page_load_capping_service->page_load_capping_blacklist()) {
      page_load_capping_service->page_load_capping_blacklist()->ClearBlackList(
          delete_begin_, delete_end_);
    }

#if defined(OS_ANDROID)
    OomInterventionDecider* oom_intervention_decider =
        OomInterventionDecider::GetForBrowserContext(profile_);
    if (oom_intervention_decider)
      oom_intervention_decider->ClearData();
#endif

    // The SSL Host State that tracks SSL interstitial "proceed" decisions may
    // include origins that the user has visited, so it must be cleared.
    // TODO(msramek): We can reuse the plugin filter here, since both plugins
    // and SSL host state are scoped to hosts and represent them as std::string.
    // Rename the method to indicate its more general usage.
    if (profile_->GetSSLHostStateDelegate()) {
      profile_->GetSSLHostStateDelegate()->Clear(
          filter_builder.IsEmptyBlacklist()
              ? base::RepeatingCallback<bool(const std::string&)>()
              : filter_builder.BuildPluginFilter());
    }

    // Clear VideoDecodePerfHistory only if asked to clear from the beginning of
    // time. The perf history is a simple summing of decode statistics with no
    // record of when the stats were written nor what site the video was played
    // on.
    if (IsForAllTime()) {
      // TODO(chcunningham): Add UMA to track how often this gets deleted.
      media::VideoDecodePerfHistory* video_decode_perf_history =
          profile_->GetVideoDecodePerfHistory();
      if (video_decode_perf_history) {
        video_decode_perf_history->ClearHistory(
            CreatePendingTaskCompletionClosure());
      }
    }

    device_event_log::Clear(delete_begin_, delete_end_);
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DOWNLOADS
  if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS) &&
      may_delete_history) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromDownloadManager(
        BrowserContext::GetDownloadManager(profile_));
    download_prefs->SetSaveFilePath(download_prefs->DownloadPath());
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_COOKIES
  // We ignore the DATA_TYPE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request DATA_TYPE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) &&
      (origin_type_mask &
       content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB)) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_CLIENT_HINTS, base::Time(), base::Time::Max(),
        base::BindRepeating(&WebsiteSettingsFilterAdapter, filter));

    // Clear the safebrowsing cookies only if time period is for "all time".  It
    // doesn't make sense to apply the time period of deleting in the last X
    // hours/days to the safebrowsing cookies since they aren't the result of
    // any user action.
    if (IsForAllTime()) {
      safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      if (sb_service) {
        network::mojom::CookieManagerPtr cookie_manager;
        sb_service->GetNetworkContext()->GetCookieManager(
            mojo::MakeRequest(&cookie_manager));

        network::mojom::CookieManager* manager_ptr = cookie_manager.get();

        network::mojom::CookieDeletionFilterPtr deletion_filter =
            filter_builder.BuildCookieDeletionFilter();
        if (!delete_begin_.is_null())
          deletion_filter->created_after_time = delete_begin_;
        if (!delete_end_.is_null())
          deletion_filter->created_before_time = delete_end_;

        manager_ptr->DeleteCookies(
            std::move(deletion_filter),
            base::BindOnce(&OnClearedCookies,
                           CreatePendingTaskCompletionClosure(),
                           std::move(cookie_manager)));
      }
    }

    MediaDeviceIDSalt::Reset(profile_->GetPrefs());
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_CONTENT_SETTINGS
  if (remove_mask & DATA_TYPE_CONTENT_SETTINGS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ContentSettings"));
    const auto* registry =
        content_settings::ContentSettingsRegistry::GetInstance();
    for (const content_settings::ContentSettingsInfo* info : *registry) {
      host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
          info->website_settings_info()->type(), delete_begin_, delete_end_,
          HostContentSettingsMap::PatternSourcePredicate());
    }

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_USB_CHOOSER_DATA, delete_begin_, delete_end_,
        HostContentSettingsMap::PatternSourcePredicate());

    auto* handler_registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(profile_);
    if (handler_registry)
      handler_registry->ClearUserDefinedHandlers(delete_begin_, delete_end_);

    ChromeTranslateClient::CreateTranslatePrefs(prefs)
        ->DeleteBlacklistedSitesBetween(delete_begin_, delete_end_);

#if !defined(OS_ANDROID)
    content::HostZoomMap* zoom_map =
        content::HostZoomMap::GetDefaultForBrowserContext(profile_);
    zoom_map->ClearZoomLevels(delete_begin_, delete_end_);
#else
    // Reset the Default Search Engine permissions to their default.
    SearchPermissionsService* search_permissions_service =
        SearchPermissionsService::Factory::GetForBrowserContext(profile_);
    search_permissions_service->ResetDSEPermissions();
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_BOOKMARKS
  if (remove_mask & DATA_TYPE_BOOKMARKS) {
    auto* bookmark_model = BookmarkModelFactory::GetForBrowserContext(profile_);
    if (bookmark_model) {
      if (delete_begin_.is_null() &&
          (delete_end_.is_null() || delete_end_.is_max())) {
        bookmark_model->RemoveAllUserBookmarks();
      } else {
        // Bookmark deletion is only implemented to remove all data after a
        // profile deletion. A full implementation would need to traverse the
        // whole tree and check timestamps against delete_begin and delete_end.
        NOTIMPLEMENTED();
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_DURABLE_PERMISSION
  if (remove_mask & DATA_TYPE_DURABLE_PERMISSION) {
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, base::Time(), base::Time::Max(),
        base::BindRepeating(&WebsiteSettingsFilterAdapter, filter));
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_SITE_USAGE_DATA
  if (remove_mask & DATA_TYPE_SITE_USAGE_DATA) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_SiteUsageData"));

    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT, base::Time(), base::Time::Max(),
        base::BindRepeating(&WebsiteSettingsFilterAdapter, filter));

    if (MediaEngagementService::IsEnabled()) {
      MediaEngagementService::Get(profile_)->ClearDataBetweenTime(delete_begin_,
                                                                  delete_end_);
    }
  }

  if ((remove_mask & DATA_TYPE_SITE_USAGE_DATA) ||
      (remove_mask & DATA_TYPE_HISTORY)) {
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_APP_BANNER, base::Time(), base::Time::Max(),
        base::BindRepeating(&WebsiteSettingsFilterAdapter, filter));

    PermissionDecisionAutoBlocker::GetForProfile(profile_)->RemoveCountsByUrl(
        filter);

#if BUILDFLAG(ENABLE_PLUGINS)
    host_content_settings_map_->ClearSettingsForOneTypeWithPredicate(
        CONTENT_SETTINGS_TYPE_PLUGINS_DATA, base::Time(), base::Time::Max(),
        base::Bind(&WebsiteSettingsFilterAdapter, filter));
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // Password manager
  if (remove_mask & DATA_TYPE_PASSWORDS) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      password_store->RemoveLoginsByURLAndTime(
          filter, delete_begin_, delete_end_,
          base::AdaptCallbackForRepeating(
              CreatePendingTaskCompletionClosure()));
    }

    BrowserContext::GetDefaultStoragePartition(profile_)
        ->GetNetworkContext()
        ->ClearHttpAuthCache(delete_begin_,
                             CreatePendingTaskCompletionClosureForMojo());

#if defined(OS_MACOSX)
    device::fido::mac::TouchIdCredentialStore(
        ChromeAuthenticatorRequestDelegate::
            TouchIdAuthenticatorConfigForProfile(profile_))
        .DeleteCredentials(delete_begin_, delete_end_);

    // When clearing passwords for all time, reset preferences that are used to
    // prevent overwriting the encryption key in the Keychain.
    if (IsForAllTime()) {
      PrefService* local_state = g_browser_process->local_state();
      if (local_state) {
        local_state->ClearPref(os_crypt::prefs::kKeyCreated);
        local_state->ClearPref(os_crypt::prefs::kKeyOverwritingPreventions);
      }
    }
#endif  // defined(OS_MACOSX)
  }

  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      password_store->DisableAutoSignInForOrigins(
          filter, base::AdaptCallbackForRepeating(
                      CreatePendingTaskCompletionClosure()));
    }
  }

  if (remove_mask & DATA_TYPE_HISTORY) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      password_store->RemoveStatisticsByOriginAndTime(
          nullable_filter, delete_begin_, delete_end_,
          base::AdaptCallbackForRepeating(
              CreatePendingTaskCompletionClosure()));
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_FORM_DATA
  // TODO(dmurph): Support all backends with filter (crbug.com/113621).
  if (remove_mask & DATA_TYPE_FORM_DATA) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
          delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(
          delete_begin_, delete_end_);

      // Clear out the Autofill StrikeDatabase in its entirety.
      // TODO(crbug.com/884817): Respect |delete_begin_| and |delete_end_| and
      // only clear out entries whose last strikes were created in that
      // timeframe.
      autofill::StrikeDatabase* strike_database =
          autofill::StrikeDatabaseFactory::GetForProfile(profile_);
      if (strike_database)
        strike_database->ClearAllStrikes(base::DoNothing());

      // Ask for a call back when the above calls are finished.
      web_data_service->GetDBTaskRunner()->PostTaskAndReply(
          FROM_HERE, base::DoNothing(), CreatePendingTaskCompletionClosure());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_CACHE
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE) {
    // Tell the renderers to clear their cache.
    // TODO(crbug.com/668114): Renderer cache is a platform concept, and should
    // live in BrowsingDataRemoverImpl. However, WebCacheManager itself is
    // a component with dependency on content/browser. Untangle these
    // dependencies or reimplement the relevant part of WebCacheManager
    // in content/browser.
    web_cache::WebCacheManager::GetInstance()->ClearCache();

#if BUILDFLAG(ENABLE_NACL)
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ClearNaClCacheOnIOThread,
                       base::AdaptCallbackForRepeating(UIThreadTrampoline(
                           CreatePendingTaskCompletionClosure()))));
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&ClearPnaclCacheOnIOThread, delete_begin_, delete_end_,
                       base::AdaptCallbackForRepeating(UIThreadTrampoline(
                           CreatePendingTaskCompletionClosure()))));
#endif

    // The PrerenderManager may have a page actively being prerendered, which
    // is essentially a preemptively cached page.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS);
    }

    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service)
      content_suggestions_service->ClearAllCachedSuggestions();

#if defined(OS_ANDROID)
#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
    if (base::FeatureList::IsEnabled(feed::kInterestFeedContentSuggestions))
      feed::FeedLifecycleBridge::ClearCachedData();
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)
#endif  // defined(OS_ANDROID)

    // |ui_nqe_service| may be null if |profile_| is not a regular profile.
    UINetworkQualityEstimatorService* ui_nqe_service =
        UINetworkQualityEstimatorServiceFactory::GetForProfile(profile_);
    DCHECK(profile_->GetProfileType() !=
               Profile::ProfileType::REGULAR_PROFILE ||
           ui_nqe_service != nullptr);
    if (ui_nqe_service) {
      // Network Quality Estimator (NQE) stores the quality (RTT, bandwidth
      // etc.) of different networks in prefs. The stored quality is not
      // broken down by URLs or timestamps, so clearing the cache should
      // completely clear the prefs.
      ui_nqe_service->ClearPrefs();
    }

    // Notify data reduction component.
    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                profile_);
    // |data_reduction_proxy_settings| is null if |profile_| is off the record.
    // Skip notification if the user clears cache for only a finite set of
    // sites.
    if (data_reduction_proxy_settings &&
        filter_builder.GetMode() != BrowsingDataFilterBuilder::WHITELIST) {
      data_reduction_proxy::DataReductionProxyService*
          data_reduction_proxy_service =
              data_reduction_proxy_settings->data_reduction_proxy_service();
      if (data_reduction_proxy_service) {
        data_reduction_proxy_service->OnCacheCleared(delete_begin_,
                                                     delete_end_);
      }
    }

#if defined(OS_ANDROID)
    // For now we're considering offline pages as cache, so if we're removing
    // cache we should remove offline pages as well.
    if ((remove_mask & content::BrowsingDataRemover::DATA_TYPE_CACHE)) {
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile_)
          ->DeleteCachedPagesByURLPredicate(
              filter,
              base::AdaptCallbackForRepeating(
                  IgnoreArgument<
                      offline_pages::OfflinePageModel::DeletePageResult>(
                      CreatePendingTaskCompletionClosure())));
    }
#endif

    // TODO(crbug.com/829321): Remove null-check.
    auto* webrtc_event_log_manager = WebRtcEventLogManager::GetInstance();
    if (webrtc_event_log_manager) {
      webrtc_event_log_manager->ClearCacheForBrowserContext(
          profile_, delete_begin_, delete_end_,
          CreatePendingTaskCompletionClosure());
    } else {
      LOG(ERROR) << "WebRtcEventLogManager not instantiated.";
    }
  }

//////////////////////////////////////////////////////////////////////////////
// DATA_TYPE_PLUGINS
// Plugins are known to //content and their bulk deletion is implemented in
// PluginDataRemover. However, the filtered deletion uses
// BrowsingDataFlashLSOHelper which (currently) has strong dependencies
// on //chrome.
// TODO(msramek): Investigate these dependencies and move the plugin deletion
// to BrowsingDataRemoverImpl in //content. Note that code in //content
// can simply take advantage of PluginDataRemover directly to delete plugin
// data in bulk.
#if BUILDFLAG(ENABLE_PLUGINS)
  // Plugin is data not separated for protected and unprotected web origins. We
  // check the origin_type_mask_ to prevent unintended deletion.
  if ((remove_mask & DATA_TYPE_PLUGIN_DATA) &&
      (origin_type_mask &
       content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB)) {
    base::RecordAction(UserMetricsAction("ClearBrowsingData_LSOData"));

    if (filter_builder.IsEmptyBlacklist()) {
      DCHECK(!plugin_data_remover_);
      plugin_data_remover_.reset(
          content::PluginDataRemover::Create(profile_));
      base::WaitableEvent* event =
          plugin_data_remover_->StartRemoving(delete_begin_);

      base::WaitableEventWatcher::EventCallback watcher_callback =
          base::BindOnce(
              &ChromeBrowsingDataRemoverDelegate::OnWaitableEventSignaled,
              weak_ptr_factory_.GetWeakPtr(),
              CreatePendingTaskCompletionClosure());
      watcher_.StartWatching(event, std::move(watcher_callback),
                             base::SequencedTaskRunnerHandle::Get());
    } else {
      // TODO(msramek): Store filters from the currently executed task on the
      // object to avoid having to copy them to callback methods.
      flash_lso_helper_->StartFetching(base::BindOnce(
          &ChromeBrowsingDataRemoverDelegate::OnSitesWithFlashDataFetched,
          weak_ptr_factory_.GetWeakPtr(), filter_builder.BuildPluginFilter(),
          CreatePendingTaskCompletionClosure()));
    }
  }
#endif

  //////////////////////////////////////////////////////////////////////////////
  // DATA_TYPE_MEDIA_LICENSES
  if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES) {
    // TODO(jrummell): This UMA should be renamed to indicate it is for Media
    // Licenses.
    base::RecordAction(UserMetricsAction("ClearBrowsingData_ContentLicenses"));

#if BUILDFLAG(ENABLE_PLUGINS)
    // Will be completed in OnDeauthorizeFlashContentLicensesCompleted()
    num_pending_tasks_ += 1;
    if (!pepper_flash_settings_manager_.get()) {
      pepper_flash_settings_manager_.reset(
          new PepperFlashSettingsManager(this, profile_));
    }
    deauthorize_flash_content_licenses_request_id_ =
        pepper_flash_settings_manager_->DeauthorizeContentLicenses(prefs);
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)
    // On Chrome OS, delete any content protection platform keys.
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (!user) {
      LOG(WARNING) << "Failed to find user for current profile.";
    } else {
      chromeos::DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->TpmAttestationDeleteKeys(
              chromeos::attestation::KEY_USER,
              cryptohome::CreateAccountIdentifierFromAccountId(
                  user->GetAccountId()),
              chromeos::attestation::kContentProtectionKeyPrefix,
              base::BindOnce(
                  &ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys,
                  weak_ptr_factory_.GetWeakPtr(),
                  CreatePendingTaskCompletionClosure()));
    }
#endif  // defined(OS_CHROMEOS)

#if defined(OS_ANDROID)
    ClearMediaDrmLicenses(prefs, delete_begin_, delete_end, filter,
                          CreatePendingTaskCompletionClosure());
#endif  // defined(OS_ANDROID);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Zero suggest.
  // Remove omnibox zero-suggest cache results. Filtering is not supported.
  // This is not a problem, as deleting more data than necessary will just cause
  // another server round-trip; no data is actually lost.
  if ((remove_mask & (content::BrowsingDataRemover::DATA_TYPE_CACHE |
                      content::BrowsingDataRemover::DATA_TYPE_COOKIES))) {
    prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());
  }

  //////////////////////////////////////////////////////////////////////////////
  // Domain reliability service.
  if (remove_mask &
      (content::BrowsingDataRemover::DATA_TYPE_COOKIES | DATA_TYPE_HISTORY)) {
    domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::
          GetForBrowserContext(profile_);
    if (service) {
      domain_reliability::DomainReliabilityClearMode mode;
      if (remove_mask & content::BrowsingDataRemover::DATA_TYPE_COOKIES)
        mode = domain_reliability::CLEAR_CONTEXTS;
      else
        mode = domain_reliability::CLEAR_BEACONS;

      service->ClearBrowsingData(mode, filter,
                                 base::AdaptCallbackForRepeating(
                                     CreatePendingTaskCompletionClosure()));
    }
  }

#if BUILDFLAG(ENABLE_REPORTING)
  if (remove_mask & DATA_TYPE_HISTORY) {
    network::mojom::NetworkContext* network_context =
        BrowserContext::GetDefaultStoragePartition(profile_)
            ->GetNetworkContext();
    network_context->ClearReportingCacheReports(
        filter_builder.BuildNetworkServiceFilter(),
        CreatePendingTaskCompletionClosureForMojo());
    network_context->ClearNetworkErrorLogging(
        filter_builder.BuildNetworkServiceFilter(),
        CreatePendingTaskCompletionClosureForMojo());
  }
#endif  // BUILDFLAG(ENABLE_REPORTING)

//////////////////////////////////////////////////////////////////////////////
// DATA_TYPE_WEB_APP_DATA
#if defined(OS_ANDROID)
  // Clear all data associated with registered webapps.
  if (remove_mask & DATA_TYPE_WEB_APP_DATA)
    webapp_registry_->UnregisterWebappsForUrls(filter);
#endif

  //////////////////////////////////////////////////////////////////////////////
  // Remove external protocol data.
  if (remove_mask & DATA_TYPE_EXTERNAL_PROTOCOL_DATA)
    ExternalProtocolHandler::ClearData(profile_);
}

void ChromeBrowsingDataRemoverDelegate::OnTaskComplete() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_GT(num_pending_tasks_, 0);
  num_pending_tasks_--;

  if (num_pending_tasks_)
    return;

  DCHECK(!callback_.is_null());
  std::move(callback_).Run();
}

base::OnceClosure
ChromeBrowsingDataRemoverDelegate::CreatePendingTaskCompletionClosure() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  num_pending_tasks_++;
  return base::BindOnce(&ChromeBrowsingDataRemoverDelegate::OnTaskComplete,
                        weak_ptr_factory_.GetWeakPtr());
}

base::OnceClosure
ChromeBrowsingDataRemoverDelegate::CreatePendingTaskCompletionClosureForMojo() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Note num_pending_tasks++ unnecessary here because it's done by the call to
  // CreatePendingTaskCompletionClosure().
  return mojo::WrapCallbackWithDropHandler(
      CreatePendingTaskCompletionClosure(),
      base::BindOnce(&ChromeBrowsingDataRemoverDelegate::OnTaskComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

#if defined(OS_ANDROID)
void ChromeBrowsingDataRemoverDelegate::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  webapp_registry_ = std::move(webapp_registry);
}
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeBrowsingDataRemoverDelegate::OverrideFlashLSOHelperForTesting(
    scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper) {
  flash_lso_helper_ = flash_lso_helper;
}
#endif

void ChromeBrowsingDataRemoverDelegate::OnKeywordsLoaded(
    base::RepeatingCallback<bool(const GURL&)> url_filter,
    base::OnceClosure done) {
  // Deletes the entries from the model.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  model->RemoveAutoGeneratedForUrlsBetween(url_filter, delete_begin_,
                                           delete_end_);
  template_url_sub_.reset();
  std::move(done).Run();
}

bool ChromeBrowsingDataRemoverDelegate::IsForAllTime() const {
  return delete_begin_ == base::Time() && delete_end_ == base::Time::Max();
}

#if defined(OS_CHROMEOS)
void ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys(
    base::OnceClosure done,
    base::Optional<bool> result) {
  LOG_IF(ERROR, !result.has_value() || !result.value())
      << "Failed to clear platform keys.";
  std::move(done).Run();
}
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeBrowsingDataRemoverDelegate::OnWaitableEventSignaled(
    base::OnceClosure done,
    base::WaitableEvent* waitable_event) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  plugin_data_remover_.reset();
  watcher_.StopWatching();
  std::move(done).Run();
}

void ChromeBrowsingDataRemoverDelegate::OnSitesWithFlashDataFetched(
    base::RepeatingCallback<bool(const std::string&)> plugin_filter,
    base::OnceClosure done,
    const std::vector<std::string>& sites) {
  std::vector<std::string> sites_to_delete;
  for (const std::string& site : sites) {
    if (plugin_filter.Run(site))
      sites_to_delete.push_back(site);
  }

  base::RepeatingClosure barrier =
      base::BarrierClosure(sites_to_delete.size(), std::move(done));

  for (const std::string& site : sites_to_delete) {
    flash_lso_helper_->DeleteFlashLSOsForSite(site, barrier);
  }
}

void ChromeBrowsingDataRemoverDelegate::
    OnDeauthorizeFlashContentLicensesCompleted(uint32_t request_id,
                                               bool /* success */) {
  DCHECK_EQ(request_id, deauthorize_flash_content_licenses_request_id_);
  OnTaskComplete();
}
#endif
