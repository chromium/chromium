// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_TEST_STATS_DICTIONARY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_TEST_STATS_DICTIONARY_H_

#include <functional>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace content {

class TestStatsDictionary;

class TestStatsReportDictionary
    : public base::RefCounted<TestStatsReportDictionary> {
 public:
  explicit TestStatsReportDictionary(
      std::unique_ptr<base::DictionaryValue> report);

  void ForEach(std::function<void(const TestStatsDictionary&)> iteration);
  std::vector<TestStatsDictionary> Filter(
      std::function<bool(const TestStatsDictionary&)> filter);

  std::unique_ptr<TestStatsDictionary> Get(const std::string& id);
  std::vector<TestStatsDictionary> GetAll();
  std::vector<TestStatsDictionary> GetByType(const std::string& type);

 private:
  friend class base::RefCounted<TestStatsReportDictionary>;
  ~TestStatsReportDictionary();

  std::unique_ptr<base::DictionaryValue> report_;
};

class TestStatsDictionary {
 public:
  TestStatsDictionary(TestStatsReportDictionary* report,
                     const base::DictionaryValue* stats);
  TestStatsDictionary(const TestStatsDictionary& other);
  ~TestStatsDictionary();

  bool IsBoolean(const std::string& key) const;
  bool GetBoolean(const std::string& key) const;

  bool IsNumber(const std::string& key) const;
  double GetNumber(const std::string& key) const;

  bool IsString(const std::string& key) const;
  std::string GetString(const std::string& key) const;

  bool IsSequenceBoolean(const std::string& key) const;
  std::vector<bool> GetSequenceBoolean(const std::string& key) const;

  bool IsSequenceNumber(const std::string& key) const;
  std::vector<double> GetSequenceNumber(const std::string& key) const;

  bool IsSequenceString(const std::string& key) const;
  std::vector<std::string> GetSequenceString(const std::string& key) const;

  std::string ToString() const;

 private:
  bool GetBoolean(const std::string& key, bool* out) const;
  bool GetNumber(const std::string& key, double* out) const;
  bool GetString(const std::string& key, std::string* out) const;
  bool GetSequenceBoolean(
      const std::string& key, std::vector<bool>* out) const;
  bool GetSequenceNumber(
      const std::string& key, std::vector<double>* out) const;
  bool GetSequenceString(
      const std::string& key, std::vector<std::string>* out) const;

  // The reference keeps the report alive which indirectly owns |stats_|.
  scoped_refptr<TestStatsReportDictionary> report_;
  const base::DictionaryValue* stats_;
};

}  // namespace content

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_TEST_STATS_DICTIONARY_H_
