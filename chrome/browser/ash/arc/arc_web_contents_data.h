// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_WEB_CONTENTS_DATA_H_
#define CHROME_BROWSER_ASH_ARC_ARC_WEB_CONTENTS_DATA_H_

#include "content/public/browser/web_contents_user_data.h"

#include "base/macros.h"

namespace arc {

// Having an object of this kind attached to a WebContents mean that the tab was
// originated via an ARC request in ChromeShellDelegate.
class ArcWebContentsData
    : public content::WebContentsUserData<ArcWebContentsData> {
 public:
  static const char kArcTransitionFlag[];

  ArcWebContentsData() = default;
  ~ArcWebContentsData() override = default;

 private:
  friend class content::WebContentsUserData<ArcWebContentsData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(ArcWebContentsData);
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_WEB_CONTENTS_DATA_H_
