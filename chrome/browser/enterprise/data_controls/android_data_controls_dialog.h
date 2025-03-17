// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ANDROID_DATA_CONTROLS_DIALOG_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ANDROID_DATA_CONTROLS_DIALOG_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/enterprise/data_controls/core/browser/data_controls_dialog.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/dialog_model.h"

namespace data_controls {

class AndroidDataControlsDialogFactory;

// Android implementation of `DataControlsDialog`, done using the modal dialog
// manager.
class AndroidDataControlsDialog : public DataControlsDialog,
                                  public content::WebContentsObserver {
 public:
  void Show(base::OnceClosure on_destructed) override;

  ~AndroidDataControlsDialog() override;

 private:
  friend AndroidDataControlsDialogFactory;

  AndroidDataControlsDialog(Type type,
                            content::WebContents* web_contents,
                            base::OnceCallback<void(bool bypassed)> callback);

  // Creates the dialog model corresponding to the action that triggered the
  // data controls.
  std::unique_ptr<ui::DialogModel> CreateDialogModel(
      base::OnceClosure on_destructed);
  std::u16string GetDialogTitle() const;
  std::u16string GetDialogLabel() const;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_ANDROID_DATA_CONTROLS_DIALOG_H_
