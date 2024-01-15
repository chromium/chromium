// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/test_stats_dictionary.h"

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/json/json_writer.h"

namespace content {

TestStatsReportDictionary::TestStatsReportDictionary(base::Value::Dict report)
    : report_(std::move(report)) {}

TestStatsReportDictionary::~TestStatsReportDictionary() = default;

void TestStatsReportDictionary::ForEach(
    std::function<void(const TestStatsDictionary&)> iteration) {
  for (auto it : report_) {
    const base::Value::Dict* it_value = it.second.GetIfDict();
    CHECK(it_value);
    iteration(TestStatsDictionary(this, it_value));
  }
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::Filter(
    std::function<bool(const TestStatsDictionary&)> filter) {
  std::vector<TestStatsDictionary> result;
  ForEach([&result, &filter](const TestStatsDictionary& stats) {
    if (filter(stats))
      result.push_back(stats);
  });
  return result;
}

std::unique_ptr<TestStatsDictionary> TestStatsReportDictionary::Get(
    const std::string& id) {
  const base::Value::Dict* dictionary = report_.FindDict(id);
  if (!dictionary)
    return nullptr;
  return std::make_unique<TestStatsDictionary>(this, dictionary);
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::GetAll() {
  return Filter([](const TestStatsDictionary&) { return true; });
}

std::vector<TestStatsDictionary> TestStatsReportDictionary::GetByType(
    const std::string& type) {
  return Filter([&type](const TestStatsDictionary& stats) {
    return stats.GetString("type") == type;
  });
}

TestStatsDictionary::TestStatsDictionary(TestStatsReportDictionary* report,
                                         const base::Value::Dict* stats)
    : report_(report), stats_(stats) {
  CHECK(report_);
  CHECK(stats_);
}

TestStatsDictionary::TestStatsDictionary(
    const TestStatsDictionary& other) = default;

TestStatsDictionary::~TestStatsDictionary() {
}

bool TestStatsDictionary::IsBoolean(const std::string& key) const {
  bool value;
  return GetBoolean(key, &value);
}

bool TestStatsDictionary::GetBoolean(const std::string& key) const {
  bool value;
  CHECK(GetBoolean(key, &value));
  return value;
}

bool TestStatsDictionary::IsNumber(const std::string& key) const {
  double value;
  return GetNumber(key, &value);
}

double TestStatsDictionary::GetNumber(const std::string& key) const {
  double value;
  CHECK(GetNumber(key, &value));
  return value;
}

bool TestStatsDictionary::IsString(const std::string& key) const {
  return stats_->FindString(key) != nullptr;
}

std::string TestStatsDictionary::GetString(const std::string& key) const {
  const std::string* value = stats_->FindString(key);
  CHECK(value);
  return *value;
}

bool TestStatsDictionary::IsSequenceBoolean(const std::string& key) const {
  std::vector<bool> value;
  return GetSequenceBoolean(key, &value);
}

std::vector<bool> TestStatsDictionary::GetSequenceBoolean(
    const std::string& key) const {
  std::vector<bool> value;
  CHECK(GetSequenceBoolean(key, &value));
  return value;
}

bool TestStatsDictionary::IsSequenceNumber(const std::string& key) const {
  std::vector<double> value;
  return GetSequenceNumber(key, &value);
}

std::vector<double> TestStatsDictionary::GetSequenceNumber(
    const std::string& key) const {
  std::vector<double> value;
  CHECK(GetSequenceNumber(key, &value));
  return value;
}

bool TestStatsDictionary::IsSequenceString(const std::string& key) const {
  std::vector<std::string> value;
  return GetSequenceString(key, &value);
}

std::vector<std::string> TestStatsDictionary::GetSequenceString(
    const std::string& key) const {
  std::vector<std::string> value;
  CHECK(GetSequenceString(key, &value));
  return value;
}

bool TestStatsDictionary::GetBoolean(
    const std::string& key, bool* out) const {
  if (std::optional<bool> value = stats_->FindBool(key)) {
    *out = *value;
    return true;
  }
  return false;
}

bool TestStatsDictionary::GetNumber(
    const std::string& key, double* out) const {
  if (std::optional<double> value = stats_->FindDouble(key)) {
    *out = *value;
    return true;
  }
  return false;
}

bool TestStatsDictionary::GetSequenceBoolean(
    const std::string& key,
    std::vector<bool>* out) const {
  const base::Value::List* list = stats_->FindList(key);
  if (!list)
    return false;
  std::vector<bool> sequence;
  for (const base::Value& arg : *list) {
    std::optional<bool> bool_value = arg.GetIfBool();
    if (!bool_value.has_value())
      return false;
    sequence.push_back(*bool_value);
  }
  *out = std::move(sequence);
  return true;
}

bool TestStatsDictionary::GetSequenceNumber(
    const std::string& key,
    std::vector<double>* out) const {
  const base::Value::List* number_sequence = stats_->FindList(key);
  if (!number_sequence)
    return false;

  out->clear();
  for (const base::Value& element : *number_sequence) {
    std::optional<double> double_value = element.GetIfDouble();
    if (!double_value)
      return false;

    out->push_back(*double_value);
  }

  return true;
}

bool TestStatsDictionary::GetSequenceString(
    const std::string& key,
    std::vector<std::string>* out) const {
  const base::Value::List* list = stats_->FindList(key);
  if (!list)
    return false;
  std::vector<std::string> sequence;
  for (const base::Value& i : *list) {
    const std::string* element = i.GetIfString();
    if (!element)
      return false;
    sequence.push_back(*element);
  }
  *out = std::move(sequence);
  return true;
}

std::string TestStatsDictionary::ToString() const {
  std::string str;
  CHECK(base::JSONWriter::WriteWithOptions(
      *stats_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &str));
  return str;
}

}  // namespace content
