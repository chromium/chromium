// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_

#include <optional>
#include <string>

#include "base/json/json_value_converter.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom.h"
#include "components/prefs/pref_registry_simple.h"

class PrefService;

namespace privacy_sandbox {

enum class SurfaceType;
class NoticeCatalog;

// TODO(crbug.com/392088228): Remove this once all values are migrated and
// histograms are migrated to use UA. This is deprecated and should only be used
// for histograms.
// LINT.IfChange(NoticeActionTaken)
enum class NoticeActionTaken {
  kMinValue = 0,
  // No Ack action set.
  kNotSet = 0,
  // ACK'ed the notice using 'GotIt' or some other form of acknowledgement.
  kAck = 1,
  // Action taken clicking the 'x' button.
  kClosed = 2,
  // TODO(crbug.com/392088228): In the process of deprecating, do not use.
  kLearnMore_Deprecated = 3,
  // Opted in/Consented to the notice using 'Turn it on' or some other form of
  // explicit consent.
  kOptIn = 4,
  // Action taken to dismiss or opt out of the notice using 'No Thanks' or some
  // other form of dismissal.
  kOptOut = 5,
  // Action taken some other way.
  kOther = 6,
  // Action taken clicking the settings button.
  kSettings = 7,
  // Action taken unknown as it was recorded pre-migration.
  kUnknownActionPreMigration = 8,
  // No action taken, the notice timed out.
  kTimedOut = 9,
  kMaxValue = kTimedOut,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/privacy/enums.xml:PrivacySandboxNoticeAction)

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

// Stores pre-migration interactions on a notice in the v1 schema.
struct V1MigrationData {
  int schema_version = 0;
  NoticeActionTaken notice_action_taken = NoticeActionTaken::kNotSet;
  base::Time notice_action_taken_time;
  base::Time notice_last_shown;

  static void RegisterJSONConverter(
      base::JSONValueConverter<V1MigrationData>* converter);
};

std::string GetNoticeActionStringFromEvent(
    notice::mojom::PrivacySandboxNoticeEvent event);

std::optional<notice::mojom::PrivacySandboxNoticeEvent> NoticeActionToEvent(
    NoticeActionTaken action);

class NoticeStorage {
 public:
  virtual ~NoticeStorage();

  // Reads PrivacySandbox notice & consent prefs. Returns std::nullopt if all
  // prefs aren't set.
  virtual std::optional<NoticeStorageData> ReadNoticeData(
      std::string_view notice) const = 0;

  // Records histograms tracking the state of all notices.
  virtual void RecordStartupHistograms() const = 0;

  // Records a Notice Event.
  virtual void RecordEvent(
      std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id,
      notice::mojom::PrivacySandboxNoticeEvent event) = 0;
};

class PrivacySandboxNoticeStorage : public NoticeStorage {
 public:
  PrivacySandboxNoticeStorage(PrefService* pref_service,
                              NoticeCatalog* catalog);
  ~PrivacySandboxNoticeStorage() override;
  PrivacySandboxNoticeStorage(const PrivacySandboxNoticeStorage&) = delete;
  PrivacySandboxNoticeStorage& operator=(const PrivacySandboxNoticeStorage&) =
      delete;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  std::optional<NoticeStorageData> ReadNoticeData(
      std::string_view notice) const override;

  void RecordStartupHistograms() const override;

  void RecordEvent(
      std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType> notice_id,
      notice::mojom::PrivacySandboxNoticeEvent event) override;

  // Migration functions.

  // Updates fields to schema version 2.
  // TODO(crbug.com/392088228): Remove this once deprecation of old V1 fields is
  // complete.
  static void UpdateNoticeSchemaV2(PrefService* pref_service);

  // Migrates fields in the notice data v1 schema to the notice data v2 schema.
  static NoticeStorageData ToV2Schema(const V1MigrationData& data_v1);

 private:
  raw_ptr<PrefService> pref_service_;
  raw_ptr<NoticeCatalog> catalog_;
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_NOTICE_STORAGE_H_
