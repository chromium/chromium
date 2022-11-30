// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_MOCK_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_MOCK_H_

#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace borealis {

class MockAnonObserver
    : public borealis::BorealisWindowManager::AnonymousAppObserver {
 public:
  MockAnonObserver();

  ~MockAnonObserver() override;

  MOCK_METHOD(void,
              OnAnonymousAppAdded,
              (const std::string&, const std::string&),
              ());

  MOCK_METHOD(void, OnAnonymousAppRemoved, (const std::string&), ());

  MOCK_METHOD(void, OnWindowManagerDeleted, (BorealisWindowManager*), ());
};

class MockLifetimeObserver
    : public borealis::BorealisWindowManager::AppWindowLifetimeObserver {
 public:
  MockLifetimeObserver();

  ~MockLifetimeObserver() override;

  MOCK_METHOD(void, OnSessionStarted, (), ());

  MOCK_METHOD(void, OnSessionFinished, (), ());

  MOCK_METHOD(void, OnAppStarted, (const std::string& app_id), ());

  MOCK_METHOD(void,
              OnAppFinished,
              (const std::string& app_id, aura::Window*),
              ());

  MOCK_METHOD(void,
              OnWindowStarted,
              (const std::string& app_id, aura::Window*),
              ());

  MOCK_METHOD(void,
              OnWindowFinished,
              (const std::string& app_id, aura::Window*),
              ());

  MOCK_METHOD(void, OnWindowManagerDeleted, (BorealisWindowManager*), ());
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_WINDOW_MANAGER_MOCK_H_
