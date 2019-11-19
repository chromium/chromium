// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data_delegate.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/common/web_application_info.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {
constexpr int kIconSize = 128;  // size of the icon in px.
const char kKeyLaunchUrl[] = "launch_url";
}  // namespace

WebKioskAppData::WebKioskAppData(KioskAppDataDelegate* delegate,
                                 const std::string& app_id,
                                 const AccountId& account_id,
                                 const GURL url)
    : KioskAppDataBase(WebKioskAppManager::kWebKioskDictionaryName,
                       app_id,
                       account_id),
      delegate_(delegate),
      status_(STATUS_INIT),
      install_url_(url) {
  name_ = install_url_.spec();
}

WebKioskAppData::~WebKioskAppData() = default;

bool WebKioskAppData::LoadFromCache() {
  PrefService* local_state = g_browser_process->local_state();
  const base::Value* dict = local_state->GetDictionary(dictionary_name());

  if (!LoadFromDictionary(base::Value::AsDictionaryValue(*dict)))
    return false;

  if (LoadLaunchUrlFromDictionary(*dict)) {
    SetStatus(STATUS_INSTALLED);
    return true;
  }

  // Wait while icon is loaded.
  if (status_ == STATUS_INIT)
    SetStatus(STATUS_LOADING);
  return true;
}

void WebKioskAppData::UpdateFromWebAppInfo(
    std::unique_ptr<WebApplicationInfo> app_info) {
  DCHECK(app_info);
  name_ = base::UTF16ToUTF8(app_info->title);
  base::FilePath cache_dir;
  if (delegate_)
    delegate_->GetKioskAppIconCacheDir(&cache_dir);

  for (const WebApplicationIconInfo& icon_info : app_info->icons) {
    if (icon_info.width == kIconSize) {
      icon_ = gfx::ImageSkia::CreateFrom1xBitmap(icon_info.data);
      icon_.MakeThreadSafe();
      SaveIcon(icon_info.data, cache_dir);
      break;
    }
  }

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate dict_update(local_state, dictionary_name());

  launch_url_ = GURL(app_info->app_url);
  const std::string app_key =
      std::string(KioskAppDataBase::kKeyApps) + '.' + app_id();
  const std::string launch_url_key = app_key + '.' + kKeyLaunchUrl;
  dict_update->SetString(launch_url_key, launch_url_.spec());
  SaveToDictionary(dict_update);

  SetStatus(STATUS_INSTALLED);
}

void WebKioskAppData::SetStatus(Status status) {
  status_ = status;

  if (delegate_)
    delegate_->OnKioskAppDataChanged(app_id());
}

bool WebKioskAppData::LoadLaunchUrlFromDictionary(const base::Value& dict) {
  const std::string app_key =
      std::string(KioskAppDataBase::kKeyApps) + '.' + app_id();
  const std::string launch_url_key = app_key + '.' + kKeyLaunchUrl;

  const std::string* launch_url_string = dict.FindStringKey(launch_url_key);
  if (!launch_url_string)
    return false;

  launch_url_ = GURL(*launch_url_string);
  return true;
}

void WebKioskAppData::OnIconLoadSuccess(const gfx::ImageSkia& icon) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  kiosk_app_icon_loader_.reset();
  icon_ = icon;
  if (status_ != STATUS_INSTALLED)
    SetStatus(STATUS_LOADED);
  else
    SetStatus(STATUS_INSTALLED);  // To notify menu controller.
}

void WebKioskAppData::OnIconLoadFailure() {
  kiosk_app_icon_loader_.reset();
  LOG(ERROR) << "Icon Load Failure";
  SetStatus(STATUS_LOADED);
  // Do nothing
}

}  // namespace chromeos
