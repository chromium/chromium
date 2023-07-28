// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_MOCK_USER_EDUCATION_DELEGATE_H_
#define ASH_USER_EDUCATION_MOCK_USER_EDUCATION_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/user_education/user_education_delegate.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial_description.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

enum class TutorialId;

// A mock implementation of the delegate of the `UserEducationController` which
// facilitates communication between Ash and user education services in the
// browser.
class ASH_EXPORT MockUserEducationDelegate : public UserEducationDelegate {
 public:
  MockUserEducationDelegate();
  MockUserEducationDelegate(const MockUserEducationDelegate&) = delete;
  MockUserEducationDelegate& operator=(const MockUserEducationDelegate&) =
      delete;
  ~MockUserEducationDelegate() override;

  // UserEducationDelegate:
  MOCK_METHOD(std::unique_ptr<user_education::HelpBubble>,
              CreateHelpBubble,
              (const AccountId& account_id,
               HelpBubbleId help_bubble_id,
               user_education::HelpBubbleParams help_bubble_params,
               ui::ElementIdentifier element_id,
               ui::ElementContext element_context),
              (override));
  MOCK_METHOD(absl::optional<ui::ElementIdentifier>,
              GetElementIdentifierForAppId,
              (const std::string& app_id),
              (const, override));
  MOCK_METHOD(const absl::optional<bool>&,
              IsNewUser,
              (const AccountId& account_id),
              (const, override));
  MOCK_METHOD(void,
              RegisterTutorial,
              (const AccountId& account_id,
               TutorialId tutorial_id,
               user_education::TutorialDescription tutorial_description),
              (override));
  MOCK_METHOD(void,
              StartTutorial,
              (const AccountId& account_id,
               TutorialId tutorial_id,
               ui::ElementContext element_context,
               base::OnceClosure completed_callback,
               base::OnceClosure aborted_callback),
              (override));
  MOCK_METHOD(void,
              AbortTutorial,
              (const AccountId& account_id,
               absl::optional<TutorialId> tutorial_id),
              (override));
  MOCK_METHOD(void,
              LaunchSystemWebAppAsync,
              (const AccountId& account_id,
               SystemWebAppType system_web_app_type,
               int64_t display_id),
              (override));
  MOCK_METHOD(bool,
              IsRunningTutorial,
              (const AccountId& account_id,
               absl::optional<TutorialId> tutorial_id),
              (const, override));
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_MOCK_USER_EDUCATION_DELEGATE_H_
