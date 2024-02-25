// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/user_education_ash_test_base.h"

#include <map>
#include <memory>
#include <optional>

#include "ash/test_shell_delegate.h"
#include "ash/user_education/mock_user_education_delegate.h"
#include "ash/user_education/user_education_types.h"
#include "base/callback_list.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {
namespace {

// Aliases.
using ::testing::Invoke;
using ::testing::WithArg;
using ::testing::WithArgs;

// RefCountedMap ---------------------------------------------------------------

// A reference counted wrapper around a `std::map<K, V>`.
template <typename K, typename V>
class RefCountedMap : public base::RefCounted<RefCountedMap<K, V>> {
 public:
  RefCountedMap() = default;
  RefCountedMap(const RefCountedMap&) = delete;
  RefCountedMap& operator=(const RefCountedMap&) = delete;

  // Returns a reference to the underlying `map_`.
  std::map<K, V>& get() { return map_; }

 private:
  friend class base::RefCounted<RefCountedMap<K, V>>;
  ~RefCountedMap() = default;
  std::map<K, V> map_;
};

}  // namespace

// UserEducationAshTestBase ----------------------------------------------------

UserEducationAshTestBase::UserEducationAshTestBase(
    base::test::TaskEnvironment::TimeSource time_source)
    : NoSessionAshTestBase(time_source) {}

void UserEducationAshTestBase::SetUp() {
  // Mock the `user_education_delegate_`.
  auto shell_delegate = std::make_unique<TestShellDelegate>();
  shell_delegate->SetUserEducationDelegateFactory(base::BindLambdaForTesting(
      [&]() -> std::unique_ptr<UserEducationDelegate> {
        // NOTE: It is expected that the `user_education_delegate_` be created
        // once and only once.
        EXPECT_EQ(user_education_delegate_, nullptr);
        auto user_education_delegate =
            std::make_unique<testing::NiceMock<MockUserEducationDelegate>>();
        user_education_delegate_ = user_education_delegate.get();

        auto aborted_callbacks_by_tutorial_id = base::MakeRefCounted<
            RefCountedMap<TutorialId, base::OnceClosureList>>();

        // Provide a default implementation for `StartTutorial()` which
        // caches `aborted_callbacks_by_tutorial_id`.
        ON_CALL(*user_education_delegate, StartTutorial)
            .WillByDefault(WithArgs<1, 4>(
                Invoke([aborted_callbacks_by_tutorial_id](
                           TutorialId tutorial_id,
                           base::OnceClosure aborted_callback) mutable {
                  aborted_callbacks_by_tutorial_id->get()[tutorial_id]
                      .AddUnsafe(std::move(aborted_callback));
                })));

        // Provide a default implementation for `AbortTutorial()` which runs
        // cached `aborted_callbacks_by_tutorial_id`.
        ON_CALL(*user_education_delegate, AbortTutorial)
            .WillByDefault(WithArg<1>(
                Invoke([aborted_callbacks_by_tutorial_id](
                           std::optional<TutorialId> tutorial_id) mutable {
                  auto it = aborted_callbacks_by_tutorial_id->get().begin();
                  while (it != aborted_callbacks_by_tutorial_id->get().end()) {
                    if (!tutorial_id || it->first == tutorial_id) {
                      it->second.Notify();
                      it = aborted_callbacks_by_tutorial_id->get().erase(it);
                      continue;
                    }
                    ++it;
                  }
                })));

        return user_education_delegate;
      }));
  NoSessionAshTestBase::SetUp(std::move(shell_delegate));
}

}  // namespace ash
