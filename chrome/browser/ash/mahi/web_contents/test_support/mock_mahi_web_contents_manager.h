// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_TEST_SUPPORT_MOCK_MAHI_WEB_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_TEST_SUPPORT_MOCK_MAHI_WEB_CONTENTS_MANAGER_H_

#include "chrome/browser/ash/mahi/web_contents/mahi_web_contents_manager_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace mahi {

class MockMahiWebContentsManager : public MahiWebContentsManagerImpl {
 public:
  MockMahiWebContentsManager();
  ~MockMahiWebContentsManager() override;

  MOCK_METHOD(void,
              OnFocusedPageLoadComplete,
              (content::WebContents*),
              (override));
};

}  // namespace mahi

#endif  // CHROME_BROWSER_ASH_MAHI_WEB_CONTENTS_TEST_SUPPORT_MOCK_MAHI_WEB_CONTENTS_MANAGER_H_
