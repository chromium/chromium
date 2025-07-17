// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_definitions.h"

namespace privacy_sandbox {
class NoticeApi;
class NoticeStorage;

// Types of notices that can be shown.
enum class NoticeType {
  kNotice,   // This type of notice requires a user to have acknowledged it.
  kConsent,  // This type of notice requires an explicit choice to be made.
};

// Notice view groups. Defining the notices that can be grouped together.
enum class NoticeViewGroup {
  kNotSet,
  kAdsNoticeEeaGroup,
};

using NoticeId = std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType>;

class Notice {
 public:
  explicit Notice(NoticeId notice_id);
  // Delete copy constructor and copy assignment operator
  Notice(const Notice&) = delete;
  Notice& operator=(const Notice&) = delete;

  // Delete move constructor and move assignment operator
  Notice(Notice&&) = delete;
  Notice& operator=(Notice&&) = delete;

  virtual ~Notice();

  // Sets Apis that need to be eligible or previously fulfilled to see this
  // notice.
  Notice* SetTargetApis(const std::vector<NoticeApi*>& apis);
  Notice* SetPreReqApis(const std::vector<NoticeApi*>& apis);
  Notice* SetFeature(const base::Feature* feature);
  Notice* SetViewGroup(std::pair<NoticeViewGroup, int> view_group);

  // Returns the cached was_fulfilled status.
  bool was_fulfilled() const { return was_fulfilled_; }

  // Accessors.
  base::span<NoticeApi*> target_apis() { return target_apis_; }
  base::span<NoticeApi*> pre_req_apis() { return pre_req_apis_; }
  NoticeId notice_id() const { return notice_id_; }
  const base::Feature* feature() const { return feature_; }

  // Returns the view_group, consisting of the group and the order in the group.
  std::pair<NoticeViewGroup, int> view_group() const { return view_group_; }

  // Update the cached was_fulfilled status for the notice.
  void RefreshFulfillmentStatus(NoticeStorage& storage);

  // Validates that all of the target apis can be fulfilled by the current
  // notice. Takes into consideration the eligibility which is only available at
  // runtime. For this reason, Fulfillemnt of each Target API has to be checked
  // at the time of computing the required notices, and not at Catalog
  // registration time.
  // Note that the eligibility check vs the notice type is
  // strict: Notice of Consent type cannot fulfil APIs that require Notice
  // Eligibility only.
  bool CanFulfillAllTargetApis();

  // Evaluates if the Notice feature is enabled.
  bool IsEnabled() const;

  const char* GetStorageName() const;

  // Gets the type of notice.
  virtual NoticeType GetNoticeType();

  // Performs post-processing on relevant target apis based on an `event`
  // performed on this notice.
  void UpdateTargetApiResults(notice::mojom::PrivacySandboxNoticeEvent event);

  // TODO(crbug.com/392612108) NoticeViews should also implement a function to
  // guard against a notice showing in certain conditions, even if it is the
  // only one that fulfills a certain Api. Example of this: Measurement Only
  // notice showing for the wrong group of users: Over 18 for example.

 private:
  // Evaluates the outcome of a notice event.
  // Return value semantics:
  // - `has_value()` is true if the event is a fulfillment event.
  // - `value()` is `true` for positive actions (Ack/OptIn) and `false` for
  // negative (OptOut).
  // - `std::nullopt` is returned for non-fulfillment events.
  // Asserts (NOTREACHED) if the event is unexpected for the Notice.
  virtual std::optional<bool> EvaluateNoticeEvent(
      notice::mojom::PrivacySandboxNoticeEvent event);

  NoticeId notice_id_;
  bool was_fulfilled_ = false;
  std::vector<NoticeApi*> target_apis_;
  std::vector<NoticeApi*> pre_req_apis_;
  raw_ptr<const base::Feature> feature_;
  std::pair<NoticeViewGroup, int> view_group_;
};

class Consent : public Notice {
 public:
  explicit Consent(NoticeId notice_id);
  NoticeType GetNoticeType() override;

 private:
  std::optional<bool> EvaluateNoticeEvent(
      notice::mojom::PrivacySandboxNoticeEvent event) override;
};

class NoticeApi {
 public:
  NoticeApi();
  // Delete copy constructor and copy assignment operator
  NoticeApi(const NoticeApi&) = delete;
  NoticeApi& operator=(const NoticeApi&) = delete;

  // Delete move constructor and move assignment operator
  NoticeApi(NoticeApi&&) = delete;
  NoticeApi& operator=(NoticeApi&&) = delete;

  virtual ~NoticeApi();

  // Accessors.
  base::span<Notice*> linked_notices() { return linked_notices_; }
  const base::Feature* feature() { return feature_; }

  // Computes the eligibility level
  EligibilityLevel GetEligibilityLevel();

  // Runs the Result Callback.
  void UpdateResult(bool enabled);

  // Sets a notice this Api can be fulfilled by.
  void SetFulfilledBy(Notice* notice);

  // Returns whether the api was fulfilled.
  // A Notice is considered fulfilled if any of its linked notices are
  // fulfilled.
  bool IsFulfilled();

  // Returns whether the API is enabled and should be considered by the
  // Orchestrator.
  bool IsEnabled();

  // Callbacks.
  NoticeApi* SetEligibilityCallback(
      base::RepeatingCallback<EligibilityLevel()> callback);
  NoticeApi* SetResultCallback(base::OnceCallback<void(bool)> callback);

  // Feature controlling the API.
  NoticeApi* SetFeature(const base::Feature* feature);

  // TODO(crbug.com/409386887) Add Supersedes call when multiple versions of the
  // same API are introduced.

 private:
  std::vector<Notice*> linked_notices_;
  base::RepeatingCallback<EligibilityLevel()> eligibility_callback_;
  base::OnceCallback<void(bool)> result_callback_;
  raw_ptr<const base::Feature> feature_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
