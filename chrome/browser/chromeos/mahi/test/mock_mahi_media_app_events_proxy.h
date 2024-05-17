// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_MEDIA_APP_EVENTS_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_MEDIA_APP_EVENTS_PROXY_H_

#include "base/unguessable_token.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace mahi {

// A mock class for testing.
class MockMahiMediaAppEventsProxy : public chromeos::MahiMediaAppEventsProxy {
 public:
  MockMahiMediaAppEventsProxy();
  MockMahiMediaAppEventsProxy(const MockMahiMediaAppEventsProxy&) = delete;
  MockMahiMediaAppEventsProxy& operator=(const MockMahiMediaAppEventsProxy&) =
      delete;
  ~MockMahiMediaAppEventsProxy() override;

  // chromeos::MahiMediaAppEventsProxy:
  MOCK_METHOD(void, OnPdfGetFocus, (const base::UnguessableToken), (override));
  MOCK_METHOD(void,
              OnPdfContextMenuShown,
              (const base::UnguessableToken, const gfx::Rect&),
              (override));
  MOCK_METHOD(void, OnPdfContextMenuHide, (), (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

}  // namespace mahi
#endif  // CHROME_BROWSER_CHROMEOS_MAHI_TEST_MOCK_MAHI_MEDIA_APP_EVENTS_PROXY_H_
