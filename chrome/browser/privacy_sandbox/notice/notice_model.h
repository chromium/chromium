// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_

#include <absl/container/flat_hash_map.h>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "chrome/browser/privacy_sandbox/notice/notice_storage.h"

namespace privacy_sandbox {
class NoticeApi;

// Types of notices that can be shown.
enum class NoticeType {
  kNotice,   // This type of notice requires a user to have acknowledged it.
  kConsent,  // This type of notice requires an explicit choice to be made.
};

// The different surface types a notice can be shown on.
enum class SurfaceType {
  kDesktopNewTab,
  kClankBrApp,      // Clank Browser App.
  kClankCustomTab,  // Clank CCT.
};

// Levels of eligibility required for a notice.
enum class EligibilityLevel {
  kNotEligible,
  kEligibleNotice,
  kEligibleConsent,
};

using NoticeId = std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType>;
class Notice {
  // TODO(crbug.com/392612108): Include view group information.
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

  bool WasFulfilled();

  // Accessors.
  const std::vector<raw_ptr<NoticeApi>>& GetTargetApis();
  const std::vector<raw_ptr<NoticeApi>>& GetPreReqApis();
  NoticeId GetNoticeId();
  const base::Feature* GetFeature();

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
  std::vector<raw_ptr<NoticeApi>> target_apis_;
  std::vector<raw_ptr<NoticeApi>> pre_req_apis_;
  raw_ptr<const base::Feature> feature_;
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
  const std::vector<Notice*>& GetLinkedNotices();
  EligibilityLevel GetEligibilityLevel();
  void UpdateResult(bool enabled);

  // TODO(crbug.com/392612108): Have enablement of an api set by a feature
  // flag.

  // Sets a notice this Api can be fulfilled by.
  void CanBeFulfilledBy(Notice* notice);

  // Returns whether the api was fulfilled.
  bool IsFulfilled();

  // Callbacks.
  NoticeApi* SetEligibilityCallback(
      base::RepeatingCallback<EligibilityLevel()> callback);
  NoticeApi* SetResultCallback(base::OnceCallback<void(bool)> callback);

 private:
  std::vector<Notice*> linked_notices_;
  base::RepeatingCallback<EligibilityLevel()> eligibility_callback_;
  base::OnceCallback<void(bool)> result_callback_;
};

using NoticeMap = absl::flat_hash_map<NoticeId, std::unique_ptr<Notice>>;
class NoticeCatalog {
 public:
  NoticeCatalog();
  ~NoticeCatalog();

  // Accessors.
  const std::vector<std::unique_ptr<NoticeApi>>& GetNoticeApis();
  const NoticeMap& GetNoticeMap();

  // Registers a new notice api.
  NoticeApi* RegisterAndRetrieveNewApi();

  // Registers a new notice.
  Notice* RegisterAndRetrieveNewNotice(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      NoticeId notice_id);

  // Registers a group of notices with the same requirements to be shown (for
  // ex. Topics can have TopicsClankBrApp, TopicsDesktop and TopicsClankCCT)
  void RegisterNoticeGroup(
      std::unique_ptr<Notice> (*notice_creator)(NoticeId),
      std::vector<std::pair<NoticeId, const base::Feature*>>&& notice_ids,
      std::vector<NoticeApi*>&& target_apis,
      std::vector<NoticeApi*>&& pre_req_apis = {});

 private:
  std::vector<std::unique_ptr<NoticeApi>> apis_;
  NoticeMap notices_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_MODEL_H_
