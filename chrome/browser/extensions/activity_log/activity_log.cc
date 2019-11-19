// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_log.h"

#include <stddef.h>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/one_shot_event.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_checker.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/counting_policy.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;

namespace extensions {

namespace {

using constants::kArgUrlPlaceholder;
using content::BrowserThread;

// If DOM API methods start with this string, we flag them as being of type
// DomActionType::XHR.
const char kDomXhrPrefix[] = "XMLHttpRequest.";

// Specifies a possible action to take to get an extracted URL in the ApiInfo
// structure below.
enum Transformation {
  NONE,
  DICT_LOOKUP,
  LOOKUP_TAB_ID,
};

// Information about specific Chrome and DOM APIs, such as which contain
// arguments that should be extracted into the arg_url field of an Action.
struct ApiInfo {
  // The lookup key consists of the action_type and api_name in the Action
  // object.
  Action::ActionType action_type;
  const char* api_name;

  // If non-negative, an index into args might contain a URL to be extracted
  // into arg_url.
  int arg_url_index;

  // A transformation to apply to the data found at index arg_url_index in the
  // argument list.
  //
  // If NONE, the data is expected to be a string which is treated as a URL.
  //
  // If LOOKUP_TAB_ID, the data is either an integer which is treated as a tab
  // ID and translated (in the context of a provided Profile), or a list of tab
  // IDs which are translated.
  //
  // If DICT_LOOKUP, the data is expected to be a dictionary, and
  // arg_url_dict_path is a path (list of keys delimited by ".") where a URL
  // string is to be found.
  Transformation arg_url_transform;
  const char* arg_url_dict_path;
};

static const ApiInfo kApiInfoTable[] = {
    // Tabs APIs that require tab ID translation
    {Action::ACTION_API_CALL, "tabs.connect", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.detectLanguage", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.duplicate", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.executeScript", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.get", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.insertCSS", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.move", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.reload", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.remove", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.sendMessage", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_CALL, "tabs.update", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onUpdated", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onMoved", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onDetached", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onAttached", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onRemoved", 0, LOOKUP_TAB_ID, NULL},
    {Action::ACTION_API_EVENT, "tabs.onReplaced", 0, LOOKUP_TAB_ID, NULL},

    // Other APIs that accept URLs as strings
    {Action::ACTION_API_CALL, "bookmarks.create", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "bookmarks.update", 1, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "cookies.get", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "cookies.getAll", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "cookies.remove", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "cookies.set", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "downloads.download", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "history.addUrl", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "history.deleteUrl", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "history.getVisits", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_API_CALL, "webstore.install", 0, NONE, NULL},
    {Action::ACTION_API_CALL, "windows.create", 0, DICT_LOOKUP, "url"},
    {Action::ACTION_DOM_ACCESS, "Document.location", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLAnchorElement.href", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLButtonElement.formAction", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLEmbedElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLFormElement.action", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLFrameElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLHtmlElement.manifest", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLIFrameElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLImageElement.longDesc", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLImageElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLImageElement.lowsrc", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLInputElement.formAction", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLInputElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLLinkElement.href", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLMediaElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLMediaElement.currentSrc", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLModElement.cite", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLObjectElement.data", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLQuoteElement.cite", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLScriptElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLSourceElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLTrackElement.src", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "HTMLVideoElement.poster", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "Location.assign", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "Location.replace", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "Window.location", 0, NONE, NULL},
    {Action::ACTION_DOM_ACCESS, "XMLHttpRequest.open", 1, NONE, NULL}};

// A singleton class which provides lookups into the kApiInfoTable data
// structure.  It inserts all data into a map on first lookup.
class ApiInfoDatabase {
 public:
  static ApiInfoDatabase* GetInstance() {
    return base::Singleton<ApiInfoDatabase>::get();
  }

  // Retrieves an ApiInfo record for the given Action type.  Returns either a
  // pointer to the record, or NULL if no such record was found.
  const ApiInfo* Lookup(Action::ActionType action_type,
                        const std::string& api_name) const {
    auto i = api_database_.find(api_name);
    if (i == api_database_.end())
      return NULL;
    if (i->second->action_type != action_type)
      return NULL;
    return i->second;
  }

 private:
  ApiInfoDatabase() {
    for (size_t i = 0; i < base::size(kApiInfoTable); i++) {
      const ApiInfo* info = &kApiInfoTable[i];
      api_database_[info->api_name] = info;
    }
  }
  virtual ~ApiInfoDatabase() {}

  // The map is keyed by API name only, since API names aren't be repeated
  // across multiple action types in kApiInfoTable.  However, the action type
  // should still be checked before returning a positive match.
  std::map<std::string, const ApiInfo*> api_database_;

  friend struct base::DefaultSingletonTraits<ApiInfoDatabase>;
  DISALLOW_COPY_AND_ASSIGN(ApiInfoDatabase);
};

// Gets the URL for a given tab ID.  Helper method for ExtractUrls.  Returns
// true if able to perform the lookup.  The URL is stored to *url, and
// *is_incognito is set to indicate whether the URL is for an incognito tab.
bool GetUrlForTabId(int tab_id,
                    Profile* profile,
                    GURL* url,
                    bool* is_incognito) {
  content::WebContents* contents = NULL;
  Browser* browser = NULL;
  bool found = ExtensionTabUtil::GetTabById(
      tab_id,
      profile,
      true,  // Search incognito tabs, too.
      &browser,
      NULL,
      &contents,
      NULL);

  if (found) {
    *url = contents->GetURL();
    *is_incognito = browser->profile()->IsOffTheRecord();
    return true;
  } else {
    return false;
  }
}

// Resolves an argument URL relative to a base page URL.  If the page URL is
// not valid, then only absolute argument URLs are supported.
bool ResolveUrl(const GURL& base, const std::string& arg, GURL* arg_out) {
  if (base.is_valid())
    *arg_out = base.Resolve(arg);
  else
    *arg_out = GURL(arg);

  return arg_out->is_valid();
}

// Performs processing of the Action object to extract URLs from the argument
// list and translate tab IDs to URLs, according to the API call metadata in
// kApiInfoTable.  Mutates the Action object in place.  There is a small chance
// that the tab id->URL translation could be wrong, if the tab has already been
// navigated by the time of invocation.
//
// Any extracted URL is stored into the arg_url field of the action, and the
// URL in the argument list is replaced with the marker value "<arg_url>".  For
// APIs that take a list of tab IDs, extracts the first valid URL into arg_url
// and overwrites the other tab IDs in the argument list with the translated
// URL.
void ExtractUrls(scoped_refptr<Action> action, Profile* profile) {
  const ApiInfo* api_info = ApiInfoDatabase::GetInstance()->Lookup(
      action->action_type(), action->api_name());
  if (api_info == NULL)
    return;

  int url_index = api_info->arg_url_index;

  if (!action->args() || url_index < 0 ||
      static_cast<size_t>(url_index) >= action->args()->GetSize())
    return;

  // Do not overwrite an existing arg_url value in the Action, so that callers
  // have the option of doing custom arg_url extraction.
  if (action->arg_url().is_valid())
    return;

  GURL arg_url;
  bool arg_incognito = action->page_incognito();

  switch (api_info->arg_url_transform) {
    case NONE: {
      // No translation needed; just extract the URL directly from a raw string
      // or from a dictionary.  Succeeds if we can find a string in the
      // argument list and that the string resolves to a valid URL.
      std::string url_string;
      if (action->args()->GetString(url_index, &url_string) &&
          ResolveUrl(action->page_url(), url_string, &arg_url)) {
        action->mutable_args()->Set(
            url_index, std::make_unique<base::Value>(kArgUrlPlaceholder));
      }
      break;
    }

    case DICT_LOOKUP: {
      CHECK(api_info->arg_url_dict_path);
      // Look up the URL from a dictionary at the specified location.  Succeeds
      // if we can find a dictionary in the argument list, the dictionary
      // contains the specified key, and the corresponding value resolves to a
      // valid URL.
      base::DictionaryValue* dict = NULL;
      std::string url_string;
      if (action->mutable_args()->GetDictionary(url_index, &dict) &&
          dict->GetString(api_info->arg_url_dict_path, &url_string) &&
          ResolveUrl(action->page_url(), url_string, &arg_url)) {
        dict->SetString(api_info->arg_url_dict_path, kArgUrlPlaceholder);
      }
      break;
    }

    case LOOKUP_TAB_ID: {
      // Translation of tab IDs to URLs has been requested.  There are two
      // cases to consider: either a single integer or a list of integers (when
      // multiple tabs are manipulated).
      int tab_id;
      base::ListValue* tab_list = NULL;
      if (action->args()->GetInteger(url_index, &tab_id)) {
        // Single tab ID to translate.
        GetUrlForTabId(tab_id, profile, &arg_url, &arg_incognito);
        if (arg_url.is_valid()) {
          action->mutable_args()->Set(
              url_index, std::make_unique<base::Value>(kArgUrlPlaceholder));
        }
      } else if (action->mutable_args()->GetList(url_index, &tab_list)) {
        // A list of possible IDs to translate.  Work through in reverse order
        // so the last one translated is left in arg_url.
        int extracted_index = -1;  // Which list item is copied to arg_url?
        for (int i = tab_list->GetSize() - 1; i >= 0; --i) {
          if (tab_list->GetInteger(i, &tab_id) &&
              GetUrlForTabId(tab_id, profile, &arg_url, &arg_incognito)) {
            if (!arg_incognito)
              tab_list->Set(i, std::make_unique<base::Value>(arg_url.spec()));
            extracted_index = i;
          }
        }
        if (extracted_index >= 0) {
          tab_list->Set(extracted_index,
                        std::make_unique<base::Value>(kArgUrlPlaceholder));
        }
      }
      break;
    }

    default:
      NOTREACHED();
  }

  if (arg_url.is_valid()) {
    action->set_arg_incognito(arg_incognito);
    action->set_arg_url(arg_url);
  }
}

// Returns the ActivityLog associated with the given |browser_context| after
// checking that |browser_context| is valid.
ActivityLog* SafeGetActivityLog(content::BrowserContext* browser_context) {
  // There's a chance that the |browser_context| was deleted some time during
  // the thread hops.
  // TODO(devlin): We should probably be doing this more extensively throughout
  // extensions code.
  if (g_browser_process->IsShuttingDown() ||
      !g_browser_process->profile_manager()->IsValidProfile(browser_context)) {
    return nullptr;
  }
  return ActivityLog::GetInstance(browser_context);
}

// Calls into the ActivityLog to log an api event or function call.
void LogApiActivity(content::BrowserContext* browser_context,
                    const std::string& extension_id,
                    const std::string& activity_name,
                    const base::ListValue& args,
                    Action::ActionType type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ActivityLogAPI::IsExtensionWhitelisted(extension_id))
    return;

  ActivityLog* activity_log = SafeGetActivityLog(browser_context);
  if (!activity_log || !activity_log->ShouldLog(extension_id))
    return;

  auto action = base::MakeRefCounted<Action>(extension_id, base::Time::Now(),
                                             type, activity_name);
  action->set_args(args.CreateDeepCopy());
  activity_log->LogAction(action);
}

// Handler for API events.
void LogApiEvent(content::BrowserContext* browser_context,
                 const std::string& extension_id,
                 const std::string& event_name,
                 const base::ListValue& args) {
  LogApiActivity(browser_context, extension_id, event_name, args,
                 Action::ACTION_API_EVENT);
}

// Handler for API function calls.
void LogApiFunction(content::BrowserContext* browser_context,
                    const std::string& extension_id,
                    const std::string& event_name,
                    const base::ListValue& args) {
  LogApiActivity(browser_context, extension_id, event_name, args,
                 Action::ACTION_API_CALL);
}

// Handler for webRequest use.
void LogWebRequestActivity(content::BrowserContext* browser_context,
                           const std::string& extension_id,
                           const GURL& url,
                           bool is_incognito,
                           const std::string& api_call,
                           std::unique_ptr<base::DictionaryValue> details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ActivityLogAPI::IsExtensionWhitelisted(extension_id))
    return;

  ActivityLog* activity_log = SafeGetActivityLog(browser_context);
  if (!activity_log || !activity_log->ShouldLog(extension_id))
    return;

  auto action = base::MakeRefCounted<Action>(
      extension_id, base::Time::Now(), Action::ACTION_WEB_REQUEST, api_call);
  action->set_page_url(url);
  action->set_page_incognito(is_incognito);
  action->mutable_other()->Set(activity_log_constants::kActionWebRequest,
                               std::move(details));
  activity_log->LogAction(action);
}

void SetActivityHandlers() {
  // Set up event handlers. We don't have to worry about unsetting these,
  // because we check whether or not the activity log is active for the context
  // in the monitor methods.
  activity_monitor::Monitor current_function_monitor =
      activity_monitor::GetApiFunctionMonitor();
  DCHECK(!current_function_monitor ||
         current_function_monitor == &LogApiFunction);
  if (!current_function_monitor)
    activity_monitor::SetApiFunctionMonitor(&LogApiFunction);

  activity_monitor::Monitor current_event_monitor =
      activity_monitor::GetApiEventMonitor();
  DCHECK(!current_event_monitor || current_event_monitor == &LogApiEvent);
  if (!current_event_monitor)
    activity_monitor::SetApiEventMonitor(&LogApiEvent);

  activity_monitor::WebRequestMonitor current_web_request_monitor =
      activity_monitor::GetWebRequestMonitor();
  DCHECK(!current_web_request_monitor ||
         current_web_request_monitor == &LogWebRequestActivity);
  if (!current_web_request_monitor)
    activity_monitor::SetWebRequestMonitor(&LogWebRequestActivity);
}

}  // namespace

// SET THINGS UP. --------------------------------------------------------------

static base::LazyInstance<BrowserContextKeyedAPIFactory<ActivityLog>>::
    DestructorAtExit g_activity_log_factory = LAZY_INSTANCE_INITIALIZER;

BrowserContextKeyedAPIFactory<ActivityLog>* ActivityLog::GetFactoryInstance() {
  return g_activity_log_factory.Pointer();
}

// static
ActivityLog* ActivityLog::GetInstance(content::BrowserContext* context) {
  return ActivityLog::GetFactoryInstance()->Get(
      Profile::FromBrowserContext(context));
}

// Use GetInstance instead of directly creating an ActivityLog.
ActivityLog::ActivityLog(content::BrowserContext* context)
    : database_policy_(NULL),
      database_policy_type_(ActivityLogPolicy::POLICY_INVALID),
      profile_(Profile::FromBrowserContext(context)),
      extension_system_(ExtensionSystem::Get(context)),
      db_enabled_(false),
      testing_mode_(false),
      active_consumers_(0),
      cached_consumer_count_(0),
      has_listeners_(false),
      is_active_(false) {
  SetActivityHandlers();

  // This controls whether logging statements are printed & which policy is set.
  testing_mode_ = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogTesting);

  // Check if the watchdog extension is previously installed and active.
  cached_consumer_count_ =
      profile_->GetPrefs()->GetInteger(prefs::kWatchdogExtensionActive);

  observers_ = base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>();

  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  CheckActive(true);  // use cached
  extension_system_->ready().Post(
      FROM_HERE, base::BindOnce(&ActivityLog::OnExtensionSystemReady,
                                weak_factory_.GetWeakPtr()));
}

void ActivityLog::SetDatabasePolicy(
    ActivityLogPolicy::PolicyType policy_type) {
  if (database_policy_type_ == policy_type)
    return;
  if (!IsDatabaseEnabled() && !IsWatchdogAppActive())
    return;

  // Deleting the old policy takes place asynchronously, on the database
  // thread.  Initializing a new policy below similarly happens
  // asynchronously.  Since the two operations are both queued for the
  // database, the queue ordering should ensure that the deletion completes
  // before database initialization occurs.
  //
  // However, changing policies at runtime is still not recommended, and
  // likely only should be done for unit tests.
  if (database_policy_)
    database_policy_->Close();

  switch (policy_type) {
    case ActivityLogPolicy::POLICY_FULLSTREAM:
      database_policy_ = new FullStreamUIPolicy(profile_);
      break;
    case ActivityLogPolicy::POLICY_COUNTS:
      database_policy_ = new CountingPolicy(profile_);
      break;
    default:
      NOTREACHED();
  }
  database_policy_->Init();
  database_policy_type_ = policy_type;
}

ActivityLog::~ActivityLog() {
  if (database_policy_)
    database_policy_->Close();
}

// MAINTAIN STATUS. ------------------------------------------------------------

void ActivityLog::ChooseDatabasePolicy() {
  if (!(IsDatabaseEnabled() || IsWatchdogAppActive()))
    return;
  if (testing_mode_)
    SetDatabasePolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
  else
    SetDatabasePolicy(ActivityLogPolicy::POLICY_COUNTS);
}

bool ActivityLog::IsDatabaseEnabled() {
  return db_enabled_;
}

bool ActivityLog::IsWatchdogAppActive() {
  return active_consumers_ > 0;
}

void ActivityLog::UpdateCachedConsumerCount() {
  cached_consumer_count_ = active_consumers_;
  profile_->GetPrefs()->SetInteger(prefs::kWatchdogExtensionActive,
                                   cached_consumer_count_);
}

void ActivityLog::SetWatchdogAppActiveForTesting(bool active) {
  active_consumers_ = active ? 1 : 0;
  CheckActive(false);  // don't use cached
}

void ActivityLog::SetHasListeners(bool has_listeners) {
  has_listeners_ = has_listeners;
  CheckActive(false);  // don't use cached
}

void ActivityLog::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  if (!ActivityLogAPI::IsExtensionWhitelisted(extension->id()))
    return;

  ++active_consumers_;

  if (!extension_system_->ready().is_signaled())
    return;

  CheckActive(false);  // don't use cached
  UpdateCachedConsumerCount();
}

void ActivityLog::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  if (!ActivityLogAPI::IsExtensionWhitelisted(extension->id()))
    return;
  --active_consumers_;

  if (!extension_system_->ready().is_signaled())
    return;

  CheckActive(false);  // don't use cached
  UpdateCachedConsumerCount();
}

// OnExtensionUnloaded will also be called right before this.
void ActivityLog::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (ActivityLogAPI::IsExtensionWhitelisted(extension->id()) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExtensionActivityLogging) &&
      active_consumers_ == 0) {
    DeleteDatabase();
  } else if (database_policy_) {
    database_policy_->RemoveExtensionData(extension->id());
  }
}

void ActivityLog::AddObserver(ActivityLog::Observer* observer) {
  observers_->AddObserver(observer);
}

void ActivityLog::RemoveObserver(ActivityLog::Observer* observer) {
  observers_->RemoveObserver(observer);
}

// static
void ActivityLog::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kWatchdogExtensionActive, false);
}

// LOG ACTIONS. ----------------------------------------------------------------

void ActivityLog::LogAction(scoped_refptr<Action> action) {
  DCHECK(ShouldLog(action->extension_id()));

  // Perform some preprocessing of the Action data: convert tab IDs to URLs and
  // mask out incognito URLs if appropriate.
  ExtractUrls(action, profile_);

  // Mark DOM XHR requests as such, for easier processing later.
  if (action->action_type() == Action::ACTION_DOM_ACCESS &&
      base::StartsWith(action->api_name(), kDomXhrPrefix,
                       base::CompareCase::SENSITIVE) &&
      action->other()) {
    base::DictionaryValue* other = action->mutable_other();
    int dom_verb = -1;
    if (other->GetInteger(constants::kActionDomVerb, &dom_verb) &&
        dom_verb == DomActionType::METHOD) {
      other->SetInteger(constants::kActionDomVerb, DomActionType::XHR);
    }
  }
  if (IsDatabaseEnabled() && database_policy_)
    database_policy_->ProcessAction(action);
  if (has_listeners_)
    observers_->Notify(FROM_HERE, &Observer::OnExtensionActivity, action);
  if (testing_mode_)
    VLOG(1) << action->PrintForDebug();
}

bool ActivityLog::ShouldLog(const std::string& extension_id) const {
  // Do not log for activities from the browser/WebUI, which is indicated by an
  // empty extension ID.
  return is_active_ && !extension_id.empty() &&
         !ActivityLogAPI::IsExtensionWhitelisted(extension_id);
}

void ActivityLog::OnScriptsExecuted(content::WebContents* web_contents,
                                    const ExecutingScriptsMap& extension_ids,
                                    const GURL& on_url) {
  if (!is_active_)
    return;
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  for (auto it = extension_ids.begin(); it != extension_ids.end(); ++it) {
    const Extension* extension =
        registry->GetExtensionById(it->first, ExtensionRegistry::ENABLED);
    if (!extension || ActivityLogAPI::IsExtensionWhitelisted(extension->id()))
      continue;

    // If OnScriptsExecuted is fired because of tabs.executeScript, the list
    // of content scripts will be empty.  We don't want to log it because
    // the call to tabs.executeScript will have already been logged anyway.
    if (!it->second.empty()) {
      scoped_refptr<Action> action;
      action = base::MakeRefCounted<Action>(extension->id(), base::Time::Now(),
                                            Action::ACTION_CONTENT_SCRIPT,
                                            "");  // no API call here
      action->set_page_url(on_url);
      action->set_page_title(base::UTF16ToUTF8(web_contents->GetTitle()));
      action->set_page_incognito(
          web_contents->GetBrowserContext()->IsOffTheRecord());

      const prerender::PrerenderManager* prerender_manager =
          prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
      if (prerender_manager &&
          prerender_manager->IsWebContentsPrerendering(web_contents, NULL))
        action->mutable_other()->SetBoolean(constants::kActionPrerender, true);
      for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
        action->mutable_args()->AppendString(*it2);
      }
      LogAction(action);
    }
  }
}

void ActivityLog::ObserveScripts(ScriptExecutor* executor) {
  executor->set_observer(base::BindRepeating(&ActivityLog::OnScriptsExecuted,
                                             weak_factory_.GetWeakPtr()));
}

// LOOKUP ACTIONS. -------------------------------------------------------------

void ActivityLog::GetFilteredActions(
    const std::string& extension_id,
    const Action::ActionType type,
    const std::string& api_name,
    const std::string& page_url,
    const std::string& arg_url,
    const int daysAgo,
    base::OnceCallback<
        void(std::unique_ptr<std::vector<scoped_refptr<Action>>>)> callback) {
  if (database_policy_) {
    database_policy_->ReadFilteredData(extension_id, type, api_name, page_url,
                                       arg_url, daysAgo, std::move(callback));
  }
}

// DELETE ACTIONS. -------------------------------------------------------------

void ActivityLog::RemoveActions(const std::vector<int64_t>& action_ids) {
  if (!database_policy_)
    return;
  database_policy_->RemoveActions(action_ids);
}

void ActivityLog::RemoveExtensionData(const std::string& extension_id) {
  if (!database_policy_)
    return;
  database_policy_->RemoveExtensionData(extension_id);
}

void ActivityLog::RemoveURLs(const std::vector<GURL>& restrict_urls) {
  if (!database_policy_)
    return;
  database_policy_->RemoveURLs(restrict_urls);
}

void ActivityLog::RemoveURLs(const std::set<GURL>& restrict_urls) {
  if (!database_policy_)
    return;

  std::vector<GURL> urls;
  for (auto it = restrict_urls.begin(); it != restrict_urls.end(); ++it) {
    urls.push_back(*it);
  }
  database_policy_->RemoveURLs(urls);
}

void ActivityLog::RemoveURL(const GURL& url) {
  if (url.is_empty())
    return;
  std::vector<GURL> urls;
  urls.push_back(url);
  RemoveURLs(urls);
}

void ActivityLog::DeleteDatabase() {
  if (!database_policy_)
    return;
  database_policy_->DeleteDatabase();
}

void ActivityLog::CheckActive(bool use_cached) {
  const bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogging);
  const bool has_consumer =
      active_consumers_ || (use_cached && cached_consumer_count_) ||
      // Only check |has_listeners_| if the switch is also present, since
      // we want to ensure the activity log is inactive unless the switch
      // or the app (covered by active_consumers_) is present.
      (has_listeners_ && has_switch);
  const bool should_be_active = has_consumer || has_switch;

  if (should_be_active == is_active_)
    return;

  bool has_db = db_enabled_ && database_policy_;
  db_enabled_ = should_be_active;

  if (should_be_active && !has_db)
    ChooseDatabasePolicy();

  is_active_ = should_be_active;

  for (content::RenderProcessHost::iterator iter(
           content::RenderProcessHost::AllHostsIterator());
       !iter.IsAtEnd(); iter.Advance()) {
    content::RenderProcessHost* host = iter.GetCurrentValue();
    if (profile_->IsSameProfile(
            Profile::FromBrowserContext(host->GetBrowserContext()))) {
      host->Send(new ExtensionMsg_SetActivityLoggingEnabled(is_active_));
    }
  }
}

void ActivityLog::OnExtensionSystemReady() {
  if (active_consumers_ != cached_consumer_count_) {
    CheckActive(false);
    UpdateCachedConsumerCount();
  }
}

template <>
void BrowserContextKeyedAPIFactory<ActivityLog>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

}  // namespace extensions
