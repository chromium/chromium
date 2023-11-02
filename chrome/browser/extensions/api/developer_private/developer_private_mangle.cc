// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/developer_private/developer_private_mangle.h"

#include <memory>

#include "base/notreached.h"
#include "base/values.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"

namespace extensions {
namespace developer_private_mangle {

api::developer_private::ItemInfo MangleExtensionInfo(
    const api::developer_private::ExtensionInfo& info) {
  api::developer_private::ItemInfo result;
  result.id = info.id;
  result.name = info.name;
  result.version = info.version;
  result.description = info.description;
  result.may_disable = info.user_may_modify;
  result.enabled =
      info.state == api::developer_private::EXTENSION_STATE_ENABLED;
  switch (info.type) {
    case api::developer_private::EXTENSION_TYPE_HOSTED_APP:
      result.type = api::developer_private::ITEM_TYPE_HOSTED_APP;
      result.is_app = true;
      break;
    case api::developer_private::EXTENSION_TYPE_PLATFORM_APP:
      result.type = api::developer_private::ITEM_TYPE_PACKAGED_APP;
      result.is_app = true;
      break;
    case api::developer_private::EXTENSION_TYPE_LEGACY_PACKAGED_APP:
      result.type = api::developer_private::ITEM_TYPE_LEGACY_PACKAGED_APP;
      result.is_app = true;
      break;
    case api::developer_private::EXTENSION_TYPE_EXTENSION:
      result.type = api::developer_private::ITEM_TYPE_EXTENSION;
      result.is_app = false;
      break;
    case api::developer_private::EXTENSION_TYPE_THEME:
      result.type = api::developer_private::ITEM_TYPE_THEME;
      result.is_app = false;
      break;
    case api::developer_private::EXTENSION_TYPE_SHARED_MODULE:
      // Old api doesn't account for this.
      break;
    default:
      NOTREACHED();
  }
  result.allow_file_access = info.file_access.is_active;
  result.wants_file_access = info.file_access.is_enabled;

  result.icon_url = info.icon_url;

  result.incognito_enabled = info.incognito_access.is_active;
  result.allow_incognito = info.incognito_access.is_enabled;

  result.is_unpacked =
      info.location == api::developer_private::LOCATION_UNPACKED;
  result.allow_reload = result.is_unpacked;
  result.terminated =
      info.state == api::developer_private::EXTENSION_STATE_TERMINATED;

  result.path = info.path;
  if (info.options_page)
    result.options_url = info.options_page->url;
  result.app_launch_url = info.launch_url;
  if (!info.home_page.url.empty())
    result.homepage_url = info.home_page.url;
  result.update_url = info.update_url;
  for (const std::string& str_warning : info.install_warnings) {
    api::developer_private::InstallWarning warning;
    warning.message = str_warning;
    result.install_warnings.push_back(std::move(warning));
  }
  for (const api::developer_private::ManifestError& error :
       info.manifest_errors) {
    base::Value::Dict value = error.ToValue();
    value.Set("type", static_cast<int>(ExtensionError::MANIFEST_ERROR));
    value.Set("level", static_cast<int>(logging::LOG_WARNING));
    result.manifest_errors.push_back(base::Value(std::move(value)));
  }
  for (const api::developer_private::RuntimeError& error :
       info.runtime_errors) {
    base::Value::Dict value = error.ToValue();
    value.Set("type", static_cast<int>(ExtensionError::RUNTIME_ERROR));
    logging::LogSeverity severity = logging::LOG_INFO;
    if (error.severity == api::developer_private::ERROR_LEVEL_WARN)
      severity = logging::LOG_WARNING;
    else if (error.severity == api::developer_private::ERROR_LEVEL_ERROR)
      severity = logging::LOG_ERROR;
    value.Set("level", static_cast<int>(severity));
    result.runtime_errors.push_back(base::Value(std::move(value)));
  }
  result.offline_enabled = info.offline_enabled;
  for (const api::developer_private::ExtensionView& view : info.views) {
    api::developer_private::ItemInspectView view_copy;
    GURL url(view.url);
    if (url.scheme() == kExtensionScheme) {
      // No leading slash.
      view_copy.path = url.path().substr(1);
    } else {
      // For live pages, use the full URL.
      view_copy.path = url.spec();
    }
    view_copy.render_process_id = view.render_process_id;
    view_copy.render_view_id = view.render_view_id;
    view_copy.incognito = view.incognito;
    view_copy.generated_background_page =
        view_copy.path == kGeneratedBackgroundPageFilename;
    result.views.push_back(std::move(view_copy));
  }

  return result;
}

}  // namespace developer_private_mangle
}  // namespace extensions
