// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/test_support/scoped_fake_default_browser_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/default_browser/default_browser_manager.h"
#include "chrome/browser/default_browser/default_browser_setter.h"
#include "chrome/browser/default_browser/test_support/fake_default_browser_monitor.h"
#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"
#include "chrome/test/base/testing_browser_process.h"

namespace default_browser {

namespace {

class AsyncFakeDefaultBrowserSetter : public DefaultBrowserSetter {
 public:
  explicit AsyncFakeDefaultBrowserSetter(DefaultBrowserSetterType type)
      : type_(type) {}

  DefaultBrowserSetterType GetType() const override { return type_; }

  void Execute(DefaultBrowserSetterCompletionCallback on_complete,
               const ExecuteParams& params) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(on_complete),
                                  DefaultBrowserState::IS_DEFAULT));
  }

 private:
  const DefaultBrowserSetterType type_;
};

}  // namespace

ScopedFakeDefaultBrowserManager::ScopedFakeDefaultBrowserManager()
    : manager_(std::make_unique<DefaultBrowserManager>(
          TestingBrowserProcess::GetGlobal(),
          std::make_unique<FakeShellDelegate>(),
          base::BindRepeating([]() -> Profile* { return nullptr; }),
          std::make_unique<FakeDefaultBrowserMonitor>(),
          base::BindRepeating([](DefaultBrowserSetterType type) {
            return std::unique_ptr<DefaultBrowserSetter>(
                std::make_unique<AsyncFakeDefaultBrowserSetter>(type));
          }))) {}

ScopedFakeDefaultBrowserManager::~ScopedFakeDefaultBrowserManager() = default;

}  // namespace default_browser
