// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_

#include <optional>
#include <string>

#include "base/json/json_value_converter.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace privacy_sandbox {

class Notice;
enum class SurfaceType;

// Different notice action outcomes. These values are persisted to logs. Entries
// should not be renumbered and numeric values should never be reused.
// LINT.IfChange(NoticeActionBehavior)
enum class NoticeActionBehavior {
  // Action taken on notice set successfully.
  kSuccess = 0,
  // Tried to set action taken before notice was shown, unexpected behavior.
  kActionBeforeShown = 1,
  // Tried to set action taken twice, unexpected behavior.
  kDuplicateActionTaken = 2,
  kMaxValue = kDuplicateActionTaken,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxNoticeActionBehavior)

struct NoticeEventTimestampPair {
  bool operator==(const NoticeEventTimestampPair& other) const = default;

  static void RegisterJSONConverter(
      base::JSONValueConverter<NoticeEventTimestampPair>* converter);

  notice::mojom::PrivacySandboxNoticeEvent event;
  base::Time timestamp;
};

struct NoticeStorageData {
  NoticeStorageData();
  ~NoticeStorageData();
  NoticeStorageData& operator=(const NoticeStorageData&) = delete;
  NoticeStorageData(const NoticeStorageData& data) = delete;
  NoticeStorageData(NoticeStorageData&& data);
  NoticeStorageData& operator=(NoticeStorageData&& data);
  bool operator==(const NoticeStorageData& other) const;

  static void RegisterJSONConverter(
      base::JSONValueConverter<NoticeStorageData>* converter);

  int schema_version = 0;
  std::string chrome_version;
  std::vector<std::unique_ptr<NoticeEventTimestampPair>> notice_events;
};

std::string GetNoticeActionStringFromEvent(
    notice::mojom::PrivacySandboxNoticeEvent event);

class NoticeStorage {
 public:
  virtual ~NoticeStorage();

  // Reads PrivacySandbox notice & consent prefs. Returns std::nullopt if all
  // prefs aren't set.
  virtual std::optional<NoticeStorageData> ReadNoticeData(
      std::string_view notice) const = 0;

  // Records histograms tracking the state of all notices.
  virtual void RecordStartupHistograms() const = 0;

  // Removes data for deprecated notices.
  virtual void CleanupDeprecatedNotices() = 0;

  // Records a Notice Event.
  virtual void RecordEvent(const Notice& notice,
                           notice::mojom::PrivacySandboxNoticeEvent event) = 0;
};

class PrivacySandboxNoticeStorage : public NoticeStorage {
 public:
  explicit PrivacySandboxNoticeStorage(PrefService* pref_service);
  ~PrivacySandboxNoticeStorage() override;
  PrivacySandboxNoticeStorage(const PrivacySandboxNoticeStorage&) = delete;
  PrivacySandboxNoticeStorage& operator=(const PrivacySandboxNoticeStorage&) =
      delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  std::optional<NoticeStorageData> ReadNoticeData(
      std::string_view notice) const override;

  void RecordStartupHistograms() const override;

  void CleanupDeprecatedNotices() override;

  void RecordEvent(const Notice& notice,
                   notice::mojom::PrivacySandboxNoticeEvent event) override;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_
