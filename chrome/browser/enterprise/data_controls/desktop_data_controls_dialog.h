// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/enterprise/data_controls/core/browser/data_controls_dialog.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class DialogDelegate;
}  // namespace views

namespace data_controls {

class DesktopDataControlsDialogFactory;

// Desktop implementation of `DataControlsDialog`, done using views.
class DesktopDataControlsDialog : public DataControlsDialog,
                                  public content::WebContentsObserver {
 public:
  // Test observer to validate the dialog was shown/closed at appropriate
  // timings, which buttons were pressed, etc. Only one `TestObserver` should be
  // instantiated per test.
  class TestObserver {
   public:
    TestObserver();
    ~TestObserver();

    // Called as the last statement in the `views::DialogDelegate` constructor.
    virtual void OnConstructed(DesktopDataControlsDialog* dialog,
                               views::DialogDelegate* dialog_delegate) {}

    // Called when `DialogDelegate::OnWidgetInitialized` is called. This is used
    // to give the test a proper hook to close the dialog after it's first
    // shown.
    virtual void OnWidgetInitialized(DesktopDataControlsDialog* dialog,
                                     views::DialogDelegate* dialog_delegate) {}

    // Called as the last statement in the `views::DialogDelegate` destructor.
    // As such, do not keep references to `dialog` or `dialog_delegate` after
    // this function returns, only use it locally to validate test assertions.
    virtual void OnDestructed(DesktopDataControlsDialog* dialog) {}
  };
  static void SetObserverForTesting(TestObserver* observer);

  void Show(base::OnceClosure on_destructed) override;

  ~DesktopDataControlsDialog() override;

  // TODO(b/351342878): These methods might be applicable to Clank as well,
  // consider refactoring this to a shared class between desktop and Clank
  // dialogs.
  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void PrimaryPageChanged(content::Page& page) override;

 private:
  friend DesktopDataControlsDialogFactory;

  DesktopDataControlsDialog(Type type,
                            content::WebContents* web_contents,
                            base::OnceCallback<void(bool bypassed)> callback);
  void CloseDialog(views::Widget::ClosedReason reason);

  std::unique_ptr<views::DialogDelegate> dialog_delegate_;
  std::unique_ptr<views::Widget> widget_;

  // Optional closure to re-enable input events, if they were ignored.
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;

  base::OnceClosure on_destructed_;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_DESKTOP_DATA_CONTROLS_DIALOG_H_
