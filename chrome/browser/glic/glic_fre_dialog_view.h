// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_FRE_DIALOG_VIEW_H_
#define CHROME_BROWSER_GLIC_GLIC_FRE_DIALOG_VIEW_H_

#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/window/dialog_delegate.h"

class Profile;

namespace glic {

// TODO(crbug.com/393401194): Refactor this to use a separate DialogDelegate and
// View, which is the preferred structure.
class GlicFreDialogView : public views::DialogDelegateView {
 public:
  GlicFreDialogView(Profile* profile, const gfx::Size& initial_size);
  GlicFreDialogView(const GlicFreDialogView&) = delete;
  GlicFreDialogView& operator=(const GlicFreDialogView&) = delete;
  ~GlicFreDialogView() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kWebViewElementIdForTesting);

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  std::unique_ptr<content::WebContents> web_contents_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_FRE_DIALOG_VIEW_H_
