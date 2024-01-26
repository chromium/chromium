// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/activity_actions.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/format_macros.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_action_constants.h"
#include "chrome/browser/extensions/activity_log/fullstream_ui_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/dom_action_types.h"
#include "url/gurl.h"

namespace constants = activity_log_constants;
namespace activity_log = extensions::api::activity_log_private;

namespace extensions {

namespace {

std::string Serialize(std::optional<base::ValueView> value) {
  std::string value_as_text;
  if (!value) {
    value_as_text = "null";
  } else {
    JSONStringValueSerializer serializer(&value_as_text);
    serializer.SerializeAndOmitBinaryValues(*value);
  }
  return value_as_text;
}

}  // namespace

using api::activity_log_private::ExtensionActivity;

Action::Action(const std::string& extension_id,
               const base::Time& time,
               const ActionType action_type,
               const std::string& api_name,
               int64_t action_id)
    : extension_id_(extension_id),
      time_(time),
      action_type_(action_type),
      api_name_(api_name),
      action_id_(action_id) {}

Action::~Action() {}

// TODO(mvrable): As an optimization, we might return this directly if the
// refcount is one.  However, there are likely to be other stray references in
// many cases that will prevent this optimization.
scoped_refptr<Action> Action::Clone() const {
  auto clone = base::MakeRefCounted<Action>(
      extension_id(), time(), action_type(), api_name(), action_id());
  if (args()) {
    clone->set_args(args()->Clone());
  }
  clone->set_page_url(page_url());
  clone->set_page_title(page_title());
  clone->set_page_incognito(page_incognito());
  clone->set_arg_url(arg_url());
  clone->set_arg_incognito(arg_incognito());
  if (other())
    clone->set_other(other()->Clone());
  return clone;
}

void Action::set_args(std::optional<base::Value::List> args) {
  args_ = std::move(args);
}

base::Value::List& Action::mutable_args() {
  if (!args_)
    args_.emplace();

  return *args_;
}

void Action::set_page_url(const GURL& page_url) {
  page_url_ = page_url;
}

void Action::set_arg_url(const GURL& arg_url) {
  arg_url_ = arg_url;
}

void Action::set_other(std::optional<base::Value::Dict> other) {
  other_ = std::move(other);
}

base::Value::Dict& Action::mutable_other() {
  if (!other_)
    other_.emplace();

  return *other_;
}

std::string Action::SerializePageUrl() const {
  return (page_incognito() ? constants::kIncognitoUrl : "") + page_url().spec();
}

void Action::ParsePageUrl(const std::string& url) {
  set_page_incognito(base::StartsWith(url, constants::kIncognitoUrl,
                                      base::CompareCase::SENSITIVE));
  if (page_incognito())
    set_page_url(GURL(url.substr(strlen(constants::kIncognitoUrl))));
  else
    set_page_url(GURL(url));
}

std::string Action::SerializeArgUrl() const {
  return (arg_incognito() ? constants::kIncognitoUrl : "") + arg_url().spec();
}

void Action::ParseArgUrl(const std::string& url) {
  set_arg_incognito(base::StartsWith(url, constants::kIncognitoUrl,
                                     base::CompareCase::SENSITIVE));
  if (arg_incognito())
    set_arg_url(GURL(url.substr(strlen(constants::kIncognitoUrl))));
  else
    set_arg_url(GURL(url));
}

ExtensionActivity Action::ConvertToExtensionActivity() {
  ExtensionActivity result;

  // We do this translation instead of using the same enum because the database
  // values need to be stable; this allows us to change the extension API
  // without affecting the database.
  switch (action_type()) {
    case ACTION_API_CALL:
      result.activity_type = activity_log::ExtensionActivityType::kApiCall;
      break;
    case ACTION_API_EVENT:
      result.activity_type = activity_log::ExtensionActivityType::kApiEvent;
      break;
    case ACTION_CONTENT_SCRIPT:
      result.activity_type =
          activity_log::ExtensionActivityType::kContentScript;
      break;
    case ACTION_DOM_ACCESS:
      result.activity_type = activity_log::ExtensionActivityType::kDomAccess;
      break;
    case ACTION_DOM_EVENT:
      result.activity_type = activity_log::ExtensionActivityType::kDomEvent;
      break;
    case ACTION_WEB_REQUEST:
      result.activity_type = activity_log::ExtensionActivityType::kWebRequest;
      break;
    case UNUSED_ACTION_API_BLOCKED:
    case ACTION_ANY:
    default:
      // This shouldn't be reached, but some people might have old or otherwise
      // weird db entries. Treat it like an API call if that happens.
      result.activity_type = activity_log::ExtensionActivityType::kApiCall;
      break;
  }

  result.extension_id = extension_id();
  result.time = time().InMillisecondsFSinceUnixEpoch();
  result.count = count();
  result.api_call = api_name();
  result.args = Serialize(args());
  if (action_id() != -1)
    result.activity_id = base::StringPrintf("%" PRId64, action_id());
  if (page_url().is_valid()) {
    if (!page_title().empty())
      result.page_title = page_title();
    result.page_url = SerializePageUrl();
  }
  if (arg_url().is_valid())
    result.arg_url = SerializeArgUrl();

  if (other()) {
    result.other.emplace();
    if (std::optional<bool> prerender =
            other()->FindBool(constants::kActionPrerender)) {
      result.other->prerender = *prerender;
    }
    if (const base::Value::Dict* web_request =
            other()->FindDict(constants::kActionWebRequest)) {
      result.other->web_request =
          ActivityLogPolicy::Util::Serialize(*web_request);
    }
    const std::string* extra = other()->FindString(constants::kActionExtra);
    if (extra)
      result.other->extra = *extra;
    if (std::optional<int> dom_verb =
            other()->FindInt(constants::kActionDomVerb)) {
      switch (static_cast<DomActionType::Type>(dom_verb.value())) {
        case DomActionType::GETTER:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kGetter;
          break;
        case DomActionType::SETTER:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kSetter;
          break;
        case DomActionType::METHOD:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kMethod;
          break;
        case DomActionType::INSERTED:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kInserted;
          break;
        case DomActionType::XHR:
          result.other->dom_verb = activity_log::ExtensionActivityDomVerb::kXhr;
          break;
        case DomActionType::WEBREQUEST:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kWebrequest;
          break;
        case DomActionType::MODIFIED:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kModified;
          break;
        default:
          result.other->dom_verb =
              activity_log::ExtensionActivityDomVerb::kNone;
      }
    } else {
      result.other->dom_verb = activity_log::ExtensionActivityDomVerb::kNone;
    }
  }

  return result;
}

std::string Action::PrintForDebug() const {
  std::string result = base::StringPrintf("ACTION ID=%" PRId64, action_id());
  result += " EXTENSION ID=" + extension_id() + " CATEGORY=";
  switch (action_type_) {
    case ACTION_API_CALL:
      result += "api_call";
      break;
    case ACTION_API_EVENT:
      result += "api_event_callback";
      break;
    case ACTION_WEB_REQUEST:
      result += "webrequest";
      break;
    case ACTION_CONTENT_SCRIPT:
      result += "content_script";
      break;
    case UNUSED_ACTION_API_BLOCKED:
      // This is deprecated.
      result += "api_blocked";
      break;
    case ACTION_DOM_EVENT:
      result += "dom_event";
      break;
    case ACTION_DOM_ACCESS:
      result += "dom_access";
      break;
    default:
      result += base::StringPrintf("type%d", static_cast<int>(action_type_));
  }

  result += " API=" + api_name_;
  if (args_) {
    result += " ARGS=" + Serialize(*args_);
  }
  if (page_url_.is_valid()) {
    if (page_incognito_)
      result += " PAGE_URL=(incognito)" + page_url_.spec();
    else
      result += " PAGE_URL=" + page_url_.spec();
  }
  if (!page_title_.empty()) {
    base::Value title(page_title_);
    result += " PAGE_TITLE=" + Serialize(title);
  }
  if (arg_url_.is_valid()) {
    if (arg_incognito_)
      result += " ARG_URL=(incognito)" + arg_url_.spec();
    else
      result += " ARG_URL=" + arg_url_.spec();
  }
  if (other_) {
    result += " OTHER=" + Serialize(*other_);
  }

  result += base::StringPrintf(" COUNT=%d", count_);
  return result;
}

bool ActionComparator::operator()(
    const scoped_refptr<Action>& lhs,
    const scoped_refptr<Action>& rhs) const {
  if (lhs->time() != rhs->time())
    return lhs->time() < rhs->time();
  else if (lhs->action_id() != rhs->action_id())
    return lhs->action_id() < rhs->action_id();
  else
    return ActionComparatorExcludingTimeAndActionId()(lhs, rhs);
}

bool ActionComparatorExcludingTimeAndActionId::operator()(
    const scoped_refptr<Action>& lhs,
    const scoped_refptr<Action>& rhs) const {
  if (lhs->extension_id() != rhs->extension_id())
    return lhs->extension_id() < rhs->extension_id();
  if (lhs->action_type() != rhs->action_type())
    return lhs->action_type() < rhs->action_type();
  if (lhs->api_name() != rhs->api_name())
    return lhs->api_name() < rhs->api_name();

  // args might be null; treat a null value as less than all non-null values,
  // including the empty string.
  if (!lhs->args() && rhs->args())
    return true;
  if (lhs->args() && !rhs->args())
    return false;
  if (lhs->args() && rhs->args()) {
    std::string lhs_args = ActivityLogPolicy::Util::Serialize(lhs->args());
    std::string rhs_args = ActivityLogPolicy::Util::Serialize(rhs->args());
    if (lhs_args != rhs_args)
      return lhs_args < rhs_args;
  }

  // Compare URLs as strings, and treat the incognito flag as a separate field.
  if (lhs->page_url().spec() != rhs->page_url().spec())
    return lhs->page_url().spec() < rhs->page_url().spec();
  if (lhs->page_incognito() != rhs->page_incognito())
    return lhs->page_incognito() < rhs->page_incognito();

  if (lhs->page_title() != rhs->page_title())
    return lhs->page_title() < rhs->page_title();

  if (lhs->arg_url().spec() != rhs->arg_url().spec())
    return lhs->arg_url().spec() < rhs->arg_url().spec();
  if (lhs->arg_incognito() != rhs->arg_incognito())
    return lhs->arg_incognito() < rhs->arg_incognito();

  // other is treated much like the args field.
  if (!lhs->other() && rhs->other())
    return true;
  if (lhs->other() && !rhs->other())
    return false;
  if (lhs->other() && rhs->other()) {
    std::string lhs_other = ActivityLogPolicy::Util::Serialize(lhs->other());
    std::string rhs_other = ActivityLogPolicy::Util::Serialize(rhs->other());
    if (lhs_other != rhs_other)
      return lhs_other < rhs_other;
  }

  // All fields compare as equal if this point is reached.
  return false;
}

}  // namespace extensions
