// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
#define ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash {

// Delegate to expose //chrome services to //components FileManagerUI.
class FileManagerUIDelegate {
 public:
  virtual ~FileManagerUIDelegate() = default;

  // Populates (writes) load time data to the source.
  virtual void PopulateLoadTimeData(content::WebUIDataSource*) const = 0;
};

}  // namespace ash

#endif  // ASH_WEBUI_FILE_MANAGER_FILE_MANAGER_UI_DELEGATE_H_
