// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/ax_inspect_factory.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/escape.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"
#include "ui/accessibility/platform/inspect/ax_tree_formatter.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/views/accessibility/view_accessibility.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/views/accessibility/widget_ax_tree_id_map.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#endif

static const char kTargetsDataFile[] = "targets-data.json";

static const char kAccessibilityModeField[] = "a11yMode";
static const char kBrowsersField[] = "browsers";
static const char kEnabledField[] = "enabled";
static const char kErrorField[] = "error";
static const char kEventLogsField[] = "eventLogs";
static const char kFaviconUrlField[] = "faviconUrl";
static const char kFlagNameField[] = "flagName";
static const char kModeIdField[] = "modeId";
static const char kNameField[] = "name";
static const char kPagesField[] = "pages";
static const char kPidField[] = "pid";
static const char kProcessIdField[] = "processId";
static const char kRequestTypeField[] = "requestType";
static const char kRoutingIdField[] = "routingId";
static const char kSessionIdField[] = "sessionId";
static const char kShouldRequestTreeField[] = "shouldRequestTree";
static const char kStartField[] = "start";
static const char kTreeField[] = "tree";
static const char kTypeField[] = "type";
static const char kUrlField[] = "url";
static const char kWidgetsField[] = "widgets";

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
static const char kWidgetIdField[] = "widgetId";
static const char kWidget[] = "widget";
#endif

// Global flags
static const char kBrowser[] = "browser";
static const char kCopyTree[] = "copyTree";
static const char kHTML[] = "html";
static const char kInternal[] = "internal";
static const char kLabelImages[] = "labelImages";
static const char kNative[] = "native";
static const char kPage[] = "page";
static const char kPDF[] = "pdf";
static const char kScreenReader[] = "screenreader";
static const char kShowOrRefreshTree[] = "showOrRefreshTree";
static const char kText[] = "text";
static const char kViewsAccessibility[] = "viewsAccessibility";
static const char kWeb[] = "web";

// Possible global flag values
static const char kDisabled[] = "disabled";
static const char kOff[] = "off";
static const char kOn[] = "on";

using ui::AXPropertyFilter;

namespace {

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    const GURL& url,
    const std::string& name,
    const GURL& favicon_url,
    int process_id,
    int routing_id,
    ui::AXMode accessibility_mode,
    base::ProcessHandle handle = base::kNullProcessHandle) {
  std::unique_ptr<base::DictionaryValue> target_data(
      new base::DictionaryValue());
  target_data->SetInteger(kProcessIdField, process_id);
  target_data->SetInteger(kRoutingIdField, routing_id);
  target_data->SetString(kUrlField, url.spec());
  target_data->SetString(kNameField, net::EscapeForHTML(name));
  target_data->SetInteger(kPidField, base::GetProcId(handle));
  target_data->SetString(kFaviconUrlField, favicon_url.spec());
  target_data->SetInteger(kAccessibilityModeField, accessibility_mode.mode());
  target_data->SetString(kTypeField, kPage);
  return target_data;
}

std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    content::RenderViewHost* rvh) {
  TRACE_EVENT1("accessibility", "BuildTargetDescriptor", "render_view_host",
               rvh);
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode accessibility_mode;

  std::string title;
  GURL url;
  GURL favicon_url;
  if (web_contents) {
    // TODO(nasko): Fix the following code to use a consistent set of data
    // across the URL, title, and favicon.
    url = web_contents->GetURL();
    title = base::UTF16ToUTF8(web_contents->GetTitle());
    content::NavigationController& controller = web_contents->GetController();
    content::NavigationEntry* entry = controller.GetVisibleEntry();
    if (entry != nullptr && entry->GetURL().is_valid()) {
      gfx::Image favicon_image = entry->GetFavicon().image;
      if (!favicon_image.IsEmpty()) {
        const SkBitmap* favicon_bitmap = favicon_image.ToSkBitmap();
        favicon_url = GURL(webui::GetBitmapDataUrl(*favicon_bitmap));
      }
    }
    accessibility_mode = web_contents->GetAccessibilityMode();
  }

  return BuildTargetDescriptor(url, title, favicon_url,
                               rvh->GetProcess()->GetID(), rvh->GetRoutingID(),
                               accessibility_mode);
}

#if !defined(OS_ANDROID)
std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(Browser* browser) {
  std::unique_ptr<base::DictionaryValue> target_data(
      new base::DictionaryValue());
  target_data->SetInteger(kSessionIdField, browser->session_id().id());
  target_data->SetString(kNameField,
                         browser->GetWindowTitleForCurrentTab(false));
  target_data->SetString(kTypeField, kBrowser);
  return target_data;
}
#endif  // !defined(OS_ANDROID)

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<base::DictionaryValue> BuildTargetDescriptor(
    views::Widget* widget) {
  std::unique_ptr<base::DictionaryValue> widget_data(
      new base::DictionaryValue());
  widget_data->SetString(kNameField,
                         widget->widget_delegate()->GetWindowTitle());
  widget_data->SetString(kTypeField, kWidget);

  // Use the Widget's root view ViewAccessibility's unique ID for lookup.
  int id = widget->GetRootView()->GetViewAccessibility().GetUniqueId().Get();
  widget_data->SetInteger(kWidgetIdField, id);
  return widget_data;
}
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)

bool ShouldHandleAccessibilityRequestCallback(const std::string& path) {
  return path == kTargetsDataFile;
}

void HandleAccessibilityRequestCallback(
    content::BrowserContext* current_context,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleAccessibilityRequestCallback(path));

  base::DictionaryValue data;
  PrefService* pref = Profile::FromBrowserContext(current_context)->GetPrefs();
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  bool is_native_enabled = content::BrowserAccessibilityState::GetInstance()
                               ->IsRendererAccessibilityEnabled();
  bool native = mode.has_mode(ui::AXMode::kNativeAPIs);
  bool web = mode.has_mode(ui::AXMode::kWebContents);
  bool text = mode.has_mode(ui::AXMode::kInlineTextBoxes);
  bool screenreader = mode.has_mode(ui::AXMode::kScreenReader);
  bool html = mode.has_mode(ui::AXMode::kHTML);
  bool pdf = mode.has_mode(ui::AXMode::kPDF);

  // The "native" and "web" flags are disabled if
  // --disable-renderer-accessibility is set.
  data.SetString(kNative,
                 is_native_enabled ? (native ? kOn : kOff) : kDisabled);
  data.SetString(kWeb, is_native_enabled ? (web ? kOn : kOff) : kDisabled);

  // The "text", "screenreader" and "html" flags are only
  // meaningful if "web" is enabled.
  bool is_web_enabled = is_native_enabled && web;
  data.SetString(kText, is_web_enabled ? (text ? kOn : kOff) : kDisabled);
  data.SetString(kScreenReader,
                 is_web_enabled ? (screenreader ? kOn : kOff) : kDisabled);
  data.SetString(kHTML, is_web_enabled ? (html ? kOn : kOff) : kDisabled);

  // The "labelImages" flag works only if "web" is enabled, the current profile
  // has the kAccessibilityImageLabelsEnabled preference set and the appropriate
  // command line switch has been used.
  bool are_accessibility_image_labels_enabled =
      is_web_enabled &&
      pref->GetBoolean(prefs::kAccessibilityImageLabelsEnabled);
  bool label_images = mode.has_mode(ui::AXMode::kLabelImages);
  data.SetString(kLabelImages, are_accessibility_image_labels_enabled
                                   ? (label_images ? kOn : kOff)
                                   : kDisabled);

  // The "pdf" flag is independent of the others.
  data.SetString(kPDF, pdf ? kOn : kOff);

  // The "Top Level Widgets" section is only relevant if views accessibility is
  // enabled.
  data.SetBoolean(kViewsAccessibility,
                  features::IsAccessibilityTreeForViewsEnabled());

  bool show_internal = pref->GetBoolean(prefs::kShowInternalAccessibilityTree);
  data.SetString(kInternal, show_internal ? kOn : kOff);

  std::unique_ptr<base::ListValue> rvh_list(new base::ListValue());
  std::unique_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());

  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    // Ignore processes that don't have a connection, such as crashed tabs.
    if (!widget->GetProcess()->IsInitializedAndNotDead())
      continue;
    content::RenderViewHost* rvh = content::RenderViewHost::From(widget);
    if (!rvh)
      continue;
    content::WebContents* web_contents =
        content::WebContents::FromRenderViewHost(rvh);
    content::WebContentsDelegate* delegate = web_contents->GetDelegate();
    if (!delegate)
      continue;
    // Ignore views that are never user-visible, like background pages.
    if (delegate->IsNeverComposited(web_contents))
      continue;
    content::BrowserContext* context = rvh->GetProcess()->GetBrowserContext();
    if (context != current_context)
      continue;

    std::unique_ptr<base::DictionaryValue> descriptor =
        BuildTargetDescriptor(rvh);
    descriptor->SetBoolean(kNative, is_native_enabled);
    descriptor->SetBoolean(kWeb, is_web_enabled);
    descriptor->SetBoolean(kLabelImages,
                           are_accessibility_image_labels_enabled);
    rvh_list->Append(std::move(descriptor));
  }
  data.Set(kPagesField, std::move(rvh_list));

  std::unique_ptr<base::ListValue> browser_list(new base::ListValue());
#if !defined(OS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    browser_list->Append(BuildTargetDescriptor(browser));
  }
#endif  // !defined(OS_ANDROID)
  data.Set(kBrowsersField, std::move(browser_list));

  std::unique_ptr<base::ListValue> widgets_list(new base::ListValue());
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (features::IsAccessibilityTreeForViewsEnabled()) {
    views::WidgetAXTreeIDMap& manager_map =
        views::WidgetAXTreeIDMap::GetInstance();
    const std::vector<views::Widget*> widgets = manager_map.GetWidgets();
    for (views::Widget* widget : widgets) {
      widgets_list->Append(BuildTargetDescriptor(widget));
    }
  }
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  data.Set(kWidgetsField, std::move(widgets_list));

  std::string json_string;
  base::JSONWriter::Write(data, &json_string);

  std::move(callback).Run(base::RefCountedString::TakeString(&json_string));
}

std::string RecursiveDumpAXPlatformNodeAsString(
    ui::AXPlatformNode* node,
    int indent,
    const std::vector<AXPropertyFilter>& property_filters) {
  if (!node)
    return "";
  std::string str(2 * indent, '+');
  std::string line = node->GetDelegate()->GetData().ToString();
  std::vector<std::string> attributes = base::SplitString(
      line, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (std::string attribute : attributes) {
    if (ui::AXTreeFormatter::MatchesPropertyFilters(property_filters, attribute,
                                                    false)) {
      str += attribute + " ";
    }
  }
  str += "\n";
  for (int i = 0; i < node->GetDelegate()->GetChildCount(); i++) {
    gfx::NativeViewAccessible child = node->GetDelegate()->ChildAtIndex(i);
    ui::AXPlatformNode* child_node =
        ui::AXPlatformNode::FromNativeViewAccessible(child);
    str += RecursiveDumpAXPlatformNodeAsString(child_node, indent + 1,
                                               property_filters);
  }
  return str;
}

// Add property filters to the property_filters vector for the given property
// filter type. The attributes are passed in as a string with each attribute
// separated by a space.
void AddPropertyFilters(std::vector<AXPropertyFilter>& property_filters,
                        const std::string& attributes,
                        AXPropertyFilter::Type type) {
  for (const std::string& attribute : base::SplitString(
           attributes, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    property_filters.emplace_back(attribute, type);
  }
}

bool IsValidJSValue(const std::string* str) {
  return str && str->length() < 5000U;
}

}  // namespace

AccessibilityUI::AccessibilityUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://accessibility source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIAccessibilityHost);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("accessibility.css", IDR_ACCESSIBILITY_CSS);
  html_source->AddResourcePath("accessibility.js", IDR_ACCESSIBILITY_JS);
  html_source->SetDefaultResource(IDR_ACCESSIBILITY_HTML);
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleAccessibilityRequestCallback),
      base::BindRepeating(&HandleAccessibilityRequestCallback,
                          web_ui->GetWebContents()->GetBrowserContext()));

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, html_source);

  web_ui->AddMessageHandler(std::make_unique<AccessibilityUIMessageHandler>());
}

AccessibilityUI::~AccessibilityUI() = default;

AccessibilityUIObserver::AccessibilityUIObserver(
    content::WebContents* web_contents,
    std::vector<std::string>* event_logs)
    : content::WebContentsObserver(web_contents), event_logs_(event_logs) {}

AccessibilityUIObserver::~AccessibilityUIObserver() = default;

void AccessibilityUIObserver::AccessibilityEventReceived(
    const content::AXEventNotificationDetails& details) {
  for (const ui::AXEvent& event : details.events) {
    event_logs_->push_back(event.ToString());
  }
}

AccessibilityUIMessageHandler::AccessibilityUIMessageHandler() = default;

AccessibilityUIMessageHandler::~AccessibilityUIMessageHandler() {
  if (!observer_)
    return;
  content::WebContents* web_contents = observer_->web_contents();
  if (!web_contents)
    return;
  StopRecording(web_contents);
}

void AccessibilityUIMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  web_ui()->RegisterMessageCallback(
      "toggleAccessibility",
      base::BindRepeating(&AccessibilityUIMessageHandler::ToggleAccessibility,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setGlobalFlag",
      base::BindRepeating(&AccessibilityUIMessageHandler::SetGlobalFlag,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestWebContentsTree",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::RequestWebContentsTree,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestNativeUITree",
      base::BindRepeating(&AccessibilityUIMessageHandler::RequestNativeUITree,
                          base::Unretained(this)));

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  web_ui()->RegisterMessageCallback(
      "requestWidgetsTree",
      base::BindRepeating(&AccessibilityUIMessageHandler::RequestWidgetsTree,
                          base::Unretained(this)));
#endif

  web_ui()->RegisterMessageCallback(
      "requestAccessibilityEvents",
      base::BindRepeating(
          &AccessibilityUIMessageHandler::RequestAccessibilityEvents,
          base::Unretained(this)));
}

void AccessibilityUIMessageHandler::ToggleAccessibility(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  int process_id = *data->FindIntPath(kProcessIdField);
  int routing_id = *data->FindIntPath(kRoutingIdField);
  int mode = *data->FindIntPath(kModeIdField);
  bool should_request_tree = *data->FindBoolPath(kShouldRequestTreeField);

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh)
    return;
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  ui::AXMode current_mode = web_contents->GetAccessibilityMode();

  if (mode & ui::AXMode::kNativeAPIs)
    current_mode.set_mode(ui::AXMode::kNativeAPIs, true);

  if (mode & ui::AXMode::kWebContents)
    current_mode.set_mode(ui::AXMode::kWebContents, true);

  if (mode & ui::AXMode::kInlineTextBoxes)
    current_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);

  if (mode & ui::AXMode::kScreenReader)
    current_mode.set_mode(ui::AXMode::kScreenReader, true);

  if (mode & ui::AXMode::kHTML)
    current_mode.set_mode(ui::AXMode::kHTML, true);

  if (mode & ui::AXMode::kLabelImages)
    current_mode.set_mode(ui::AXMode::kLabelImages, true);

  web_contents->SetAccessibilityMode(current_mode);

  if (should_request_tree) {
    base::DictionaryValue request_data;
    request_data.SetIntPath(kProcessIdField, process_id);
    request_data.SetIntPath(kRoutingIdField, routing_id);
    request_data.SetStringPath(kRequestTypeField, kShowOrRefreshTree);
    base::ListValue request_args;
    request_args.Append(std::move(request_data));
    RequestWebContentsTree(&request_args);
  } else {
    // Call accessibility.showOrRefreshTree without a 'tree' field so the row's
    // accessibility mode buttons are updated.
    AllowJavascript();
    std::unique_ptr<base::DictionaryValue> new_mode(BuildTargetDescriptor(rvh));
    FireWebUIListener("showOrRefreshTree", *(new_mode.get()));
  }
}

void AccessibilityUIMessageHandler::SetGlobalFlag(const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  const std::string* flag_name_str_p = data->FindStringPath(kFlagNameField);
  CHECK(IsValidJSValue(flag_name_str_p));
  std::string flag_name_str = *flag_name_str_p;
  bool enabled = *data->FindBoolPath(kEnabledField);

  AllowJavascript();
  if (flag_name_str == kInternal) {
    PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
    pref->SetBoolean(prefs::kShowInternalAccessibilityTree, enabled);
    return;
  }

  ui::AXMode new_mode;
  if (flag_name_str == kNative) {
    new_mode = ui::AXMode::kNativeAPIs;
  } else if (flag_name_str == kWeb) {
    new_mode = ui::AXMode::kWebContents;
  } else if (flag_name_str == kText) {
    new_mode = ui::AXMode::kInlineTextBoxes;
  } else if (flag_name_str == kScreenReader) {
    new_mode = ui::AXMode::kScreenReader;
  } else if (flag_name_str == kHTML) {
    new_mode = ui::AXMode::kHTML;
  } else if (flag_name_str == kLabelImages) {
    new_mode = ui::AXMode::kLabelImages;
  } else {
    NOTREACHED();
    return;
  }

  // It doesn't make sense to enable one of the flags that depends on
  // web contents without enabling web contents accessibility too.
  if (enabled && (new_mode.has_mode(ui::AXMode::kInlineTextBoxes) ||
                  new_mode.has_mode(ui::AXMode::kScreenReader) ||
                  new_mode.has_mode(ui::AXMode::kHTML) ||
                  new_mode.has_mode(ui::AXMode::kLabelImages))) {
    new_mode.set_mode(ui::AXMode::kWebContents, true);
  }

  // Similarly if you disable web accessibility we should remove all
  // flags that depend on it.
  if (!enabled && new_mode.has_mode(ui::AXMode::kWebContents)) {
    new_mode.set_mode(ui::AXMode::kInlineTextBoxes, true);
    new_mode.set_mode(ui::AXMode::kScreenReader, true);
    new_mode.set_mode(ui::AXMode::kHTML, true);
  }

  content::BrowserAccessibilityState* state =
      content::BrowserAccessibilityState::GetInstance();
  if (enabled)
    state->AddAccessibilityModeFlags(new_mode);
  else
    state->RemoveAccessibilityModeFlags(new_mode);
}

void AccessibilityUIMessageHandler::GetRequestTypeAndFilters(
    const base::DictionaryValue* data,
    std::string& request_type,
    std::string& allow,
    std::string& allow_empty,
    std::string& deny) {
  DCHECK(data);
  const std::string* request_type_p = data->FindStringPath(kRequestTypeField);
  CHECK(IsValidJSValue(request_type_p));
  request_type = *request_type_p;
  CHECK(request_type == kShowOrRefreshTree || request_type == kCopyTree);

  const std::string* allow_p = data->FindStringPath("filters.allow");
  CHECK(IsValidJSValue(allow_p));
  allow = *allow_p;
  const std::string* allow_empty_p = data->FindStringPath("filters.allowEmpty");
  CHECK(IsValidJSValue(allow_empty_p));
  allow_empty = *allow_empty_p;
  const std::string* deny_p = data->FindStringPath("filters.deny");
  CHECK(IsValidJSValue(deny_p));
  deny = *deny_p;
}

void AccessibilityUIMessageHandler::RequestWebContentsTree(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  int process_id = *data->FindIntPath(kProcessIdField);
  int routing_id = *data->FindIntPath(kRoutingIdField);

  AllowJavascript();
  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh) {
    std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
    result->SetInteger(kProcessIdField, process_id);
    result->SetInteger(kRoutingIdField, routing_id);
    result->SetString(kErrorField, "Renderer no longer exists.");
    FireWebUIListener(request_type, *(result.get()));
    return;
  }

  std::unique_ptr<base::DictionaryValue> result(BuildTargetDescriptor(rvh));
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  // No matter the state of the current web_contents, we want to force the mode
  // because we are about to show the accessibility tree
  web_contents->SetAccessibilityMode(ui::AXMode(ui::kAXModeBasic));
  // Enable AXMode to access to AX objects.
  ui::AXPlatformNode::NotifyAddAXModeFlags(ui::kAXModeComplete);

  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  PrefService* pref = Profile::FromWebUI(web_ui())->GetPrefs();
  bool internal = pref->GetBoolean(prefs::kShowInternalAccessibilityTree);
  std::string accessibility_contents =
      web_contents->DumpAccessibilityTree(internal, property_filters);
  result->SetString(kTreeField, accessibility_contents);
  FireWebUIListener(request_type, *(result.get()));
}

void AccessibilityUIMessageHandler::RequestNativeUITree(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  int session_id = *data->FindIntPath(kSessionIdField);

  AllowJavascript();

#if !defined(OS_ANDROID)
  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->session_id().id() == session_id) {
      std::unique_ptr<base::DictionaryValue> result(
          BuildTargetDescriptor(browser));
      gfx::NativeWindow native_window = browser->window()->GetNativeWindow();
      ui::AXPlatformNode* node =
          ui::AXPlatformNode::FromNativeWindow(native_window);
      result->SetKey(kTreeField,
                     base::Value(RecursiveDumpAXPlatformNodeAsString(
                         node, 0, property_filters)));
      FireWebUIListener(request_type, *(result.get()));
      return;
    }
  }
#endif  // !defined(OS_ANDROID)
  // No browser with the specified |session_id| was found.
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetInteger(kSessionIdField, session_id);
  result->SetString(kTypeField, kBrowser);
  result->SetString(kErrorField, "Browser no longer exists.");
  FireWebUIListener(request_type, *(result.get()));
}

void AccessibilityUIMessageHandler::RequestWidgetsTree(
    const base::ListValue* args) {
#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  std::string request_type, allow, allow_empty, deny;
  GetRequestTypeAndFilters(data, request_type, allow, allow_empty, deny);

  std::vector<AXPropertyFilter> property_filters;
  AddPropertyFilters(property_filters, allow, AXPropertyFilter::ALLOW);
  AddPropertyFilters(property_filters, allow_empty,
                     AXPropertyFilter::ALLOW_EMPTY);
  AddPropertyFilters(property_filters, deny, AXPropertyFilter::DENY);

  if (features::IsAccessibilityTreeForViewsEnabled()) {
    int widget_id = *data->FindIntPath(kWidgetIdField);
    views::WidgetAXTreeIDMap& manager_map =
        views::WidgetAXTreeIDMap::GetInstance();
    const std::vector<views::Widget*> widgets = manager_map.GetWidgets();
    for (views::Widget* widget : widgets) {
      int current_id =
          widget->GetRootView()->GetViewAccessibility().GetUniqueId().Get();
      if (current_id == widget_id) {
        ui::AXTreeID tree_id = manager_map.GetWidgetTreeID(widget);
        DCHECK_NE(tree_id, ui::AXTreeIDUnknown());
        std::unique_ptr<ui::AXTreeFormatter> formatter(
            content::AXInspectFactory::CreateBlinkFormatter());
        std::string tree_dump =
            formatter->DumpInternalAccessibilityTree(tree_id, property_filters);

        std::unique_ptr<base::DictionaryValue> result(
            BuildTargetDescriptor(widget));
        result->SetKey(kTreeField, base::Value(tree_dump));
        AllowJavascript();
        FireWebUIListener(request_type, *(result.get()));
        return;
      }
    }
  }

  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());
  result->SetString(kTypeField, kWidget);
  result->SetString(kErrorField, "Window no longer exists.");
  AllowJavascript();
  FireWebUIListener(request_type, *(result.get()));
#endif  // defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void AccessibilityUIMessageHandler::Callback(const std::string& str) {
  event_logs_.push_back(str);
}

void AccessibilityUIMessageHandler::StopRecording(
    content::WebContents* web_contents) {
  web_contents->RecordAccessibilityEvents(false, base::nullopt);
  observer_.reset(nullptr);
}

void AccessibilityUIMessageHandler::RequestAccessibilityEvents(
    const base::ListValue* args) {
  const base::DictionaryValue* data;
  CHECK(args->GetDictionary(0, &data));

  int process_id = *data->FindIntPath(kProcessIdField);
  int routing_id = *data->FindIntPath(kRoutingIdField);
  bool start_recording = *data->FindBoolPath(kStartField);

  AllowJavascript();

  content::RenderViewHost* rvh =
      content::RenderViewHost::FromID(process_id, routing_id);
  if (!rvh) {
    return;
  }

  std::unique_ptr<base::DictionaryValue> result(BuildTargetDescriptor(rvh));
  content::WebContents* web_contents =
      content::WebContents::FromRenderViewHost(rvh);
  if (start_recording) {
    if (observer_) {
      return;
    }
    web_contents->RecordAccessibilityEvents(
        true, base::BindRepeating(&AccessibilityUIMessageHandler::Callback,
                                  base::Unretained(this)));
    observer_ =
        std::make_unique<AccessibilityUIObserver>(web_contents, &event_logs_);
  } else {
    StopRecording(web_contents);

    std::string event_logs_str;
    for (std::string log : event_logs_) {
      event_logs_str += log;
      event_logs_str += "\n";
    }
    result->SetString(kEventLogsField, event_logs_str);
    event_logs_.clear();

    FireWebUIListener("startOrStopEvents", *(result.get()));
  }
}

// static
void AccessibilityUIMessageHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kShowInternalAccessibilityTree, false);
}
