// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FONT_ACCESS_CHROME_FONT_ACCESS_DELEGATE_H_
#define CHROME_BROWSER_FONT_ACCESS_CHROME_FONT_ACCESS_DELEGATE_H_

#include "content/public/browser/font_access_chooser.h"
#include "content/public/browser/font_access_delegate.h"

class ChromeFontAccessDelegate : public content::FontAccessDelegate {
 public:
  ChromeFontAccessDelegate();
  ~ChromeFontAccessDelegate() override;

  ChromeFontAccessDelegate(ChromeFontAccessDelegate&) = delete;
  ChromeFontAccessDelegate& operator=(ChromeFontAccessDelegate&) = delete;

  std::unique_ptr<content::FontAccessChooser> RunChooser(
      content::RenderFrameHost* frame,
      const std::vector<std::string>& selection,
      content::FontAccessChooser::Callback callback) override;
};

#endif  // CHROME_BROWSER_FONT_ACCESS_CHROME_FONT_ACCESS_DELEGATE_H_
