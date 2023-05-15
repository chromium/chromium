// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_ID_GENERATOR_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_ID_GENERATOR_H_

#include <string>
#include <vector>

namespace ash {

// A class to generate IDs.
class IdGenerator {
 public:
  virtual ~IdGenerator() = default;

  virtual std::string GenerateId() = 0;
};

// Generates IDs using |base::Uuid::GenerateRandomV4().AsLowercaseString()|.
class GuidIdGenerator : public IdGenerator {
 public:
  GuidIdGenerator() = default;
  GuidIdGenerator(const GuidIdGenerator&) = delete;
  GuidIdGenerator& operator=(const GuidIdGenerator&) = delete;
  ~GuidIdGenerator() override = default;

  // IdGenerator:
  std::string GenerateId() override;
};

class FakeIdGenerator : public IdGenerator {
 public:
  explicit FakeIdGenerator(const std::vector<std::string>& ids);
  FakeIdGenerator(const FakeIdGenerator&) = delete;
  FakeIdGenerator& operator=(const FakeIdGenerator&) = delete;
  ~FakeIdGenerator() override;

  // IdGenerator:
  std::string GenerateId() override;

 private:
  std::vector<std::string> ids_;
  int index_ = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_ID_GENERATOR_H_
