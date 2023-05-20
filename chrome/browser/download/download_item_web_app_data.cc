// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_item_web_app_data.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/download/public/common/download_item.h"

const char DownloadItemWebAppData::kKey[] =
    "DownloadItem DownloadItemWebAppData";

// static
DownloadItemWebAppData* DownloadItemWebAppData::Get(
    download::DownloadItem* item) {
  base::SupportsUserData::Data* data = item->GetUserData(kKey);
  return data ? static_cast<DownloadItemWebAppData*>(data) : nullptr;
}

// static
void DownloadItemWebAppData::CreateAndAttachToItem(
    download::DownloadItem* item,
    const web_app::AppId& web_app_id) {
  auto* data = new DownloadItemWebAppData(web_app_id);
  item->SetUserData(kKey, base::WrapUnique(data));
}

DownloadItemWebAppData::DownloadItemWebAppData(const web_app::AppId& web_app_id)
    : web_app_id_(web_app_id) {}
