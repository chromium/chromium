// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_SPECIALIZED_LOG_SOURCES_H_
#define CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_SPECIALIZED_LOG_SOURCES_H_

#include "base/time/time.h"
#include "chrome/browser/ash/chromebox_for_meetings/artemis/log_source.h"
#include "third_party/re2/src/re2/re2.h"

namespace ash::cfm {

inline constexpr char kCfmAuditLogFile[] = "/var/log/audit/audit.log";
inline constexpr char kCfmBiosInfoLogFile[] = "/var/log/bios_info.txt";
inline constexpr char kCfmEventlogLogFile[] = "/var/log/eventlog.txt";

// The full paths for the sources below will be determined at runtime
inline constexpr char kCfmLacrosLogFile[] = "lacros.log";
inline constexpr char kCfmVariationsListLogFile[] = ".variations-list.txt";

// audit.log
class AuditLogSource : public LogSource {
 public:
  AuditLogSource(base::TimeDelta poll_rate, size_t batch_size);
  AuditLogSource(const AuditLogSource&) = delete;
  AuditLogSource& operator=(const AuditLogSource&) = delete;
  ~AuditLogSource() override;

 protected:
  // LocalDataSource:
  RE2& GetLogLineRegex() override;
  uint64_t TimestampStringToUnixTime(const std::string& timestamp) override;
};

// bios_info.txt
class BiosInfoLogSource : public LogSource {
 public:
  BiosInfoLogSource(base::TimeDelta poll_rate, size_t batch_size);
  BiosInfoLogSource(const BiosInfoLogSource&) = delete;
  BiosInfoLogSource& operator=(const BiosInfoLogSource&) = delete;
  ~BiosInfoLogSource() override;

 protected:
  // LocalDataSource:
  bool AreTimestampsExpected() const override;
};

// eventlog.txt
class EventlogLogSource : public LogSource {
 public:
  EventlogLogSource(base::TimeDelta poll_rate, size_t batch_size);
  EventlogLogSource(const EventlogLogSource&) = delete;
  EventlogLogSource& operator=(const EventlogLogSource&) = delete;
  ~EventlogLogSource() override;

 protected:
  // LocalDataSource:
  RE2& GetLogLineRegex() override;
};

// lacros.log
class LacrosLogSource : public LogSource {
 public:
  LacrosLogSource(base::TimeDelta poll_rate, size_t batch_size);
  LacrosLogSource(const LacrosLogSource&) = delete;
  LacrosLogSource& operator=(const LacrosLogSource&) = delete;
  ~LacrosLogSource() override;

 private:
  std::string GetLacrosLogPath(const std::string& basename);
};

// .variations-list.txt
class VariationsListLogSource : public LogSource {
 public:
  VariationsListLogSource(base::TimeDelta poll_rate, size_t batch_size);
  VariationsListLogSource(const VariationsListLogSource&) = delete;
  VariationsListLogSource& operator=(const VariationsListLogSource&) = delete;
  ~VariationsListLogSource() override;

 protected:
  // LocalDataSource:
  bool AreTimestampsExpected() const override;

 private:
  std::string GetVariationsLogPath(const std::string& basename);
};

}  // namespace ash::cfm
#endif  // CHROME_BROWSER_ASH_CHROMEBOX_FOR_MEETINGS_ARTEMIS_SPECIALIZED_LOG_SOURCES_H_
