// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_FACTORY_H_

#include "components/enterprise/data_controls/core/browser/data_controls_dialog_factory.h"

namespace data_controls {

class DesktopDataControlsDialogFactory : public DataControlsDialogFactory {
 public:
  static DesktopDataControlsDialogFactory* GetInstance();

  DesktopDataControlsDialogFactory() = default;
  virtual ~DesktopDataControlsDialogFactory() = default;

 private:
  DataControlsDialog* CreateDialog(
      DataControlsDialog::Type type,
      content::WebContents* web_contents,
      base::OnceCallback<void(bool bypassed)> callback) override;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_FACTORY_H_
