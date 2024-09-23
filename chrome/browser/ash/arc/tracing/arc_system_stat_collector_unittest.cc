// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/tracing/arc_system_stat_collector.h"

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

using ArcSystemStatCollectorTest = testing::Test;

namespace {

base::ScopedFD OpenTestStatFile(const std::string& name) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath path =
      base_path.Append("arc_overview_tracing").Append(name);
  base::ScopedFD result(open(path.value().c_str(), O_RDONLY));
  DCHECK_GE(result.get(), 0);
  return result;
}

}  // namespace

TEST_F(ArcSystemStatCollectorTest, Parse) {
  int64_t zram_values[3];
  EXPECT_TRUE(ParseStatFile(OpenTestStatFile("zram_stat").get(),
                            ArcSystemStatCollector::kZramStatColumns,
                            zram_values));
  EXPECT_EQ(2384, zram_values[0]);
  EXPECT_EQ(56696, zram_values[1]);
  EXPECT_EQ(79, zram_values[2]);

  int64_t mem_values[2];
  EXPECT_TRUE(ParseStatFile(OpenTestStatFile("proc_meminfo").get(),
                            ArcSystemStatCollector::kMemInfoColumns,
                            mem_values));
  EXPECT_EQ(8058940, mem_values[0]);
  EXPECT_EQ(2714260, mem_values[1]);

  int64_t gem_values[2];
  EXPECT_TRUE(ParseStatFile(OpenTestStatFile("gem_objects").get(),
                            ArcSystemStatCollector::kGemInfoColumns,
                            gem_values));
  EXPECT_EQ(853, gem_values[0]);
  EXPECT_EQ(458256384, gem_values[1]);

  int64_t cpu_temp;
  EXPECT_TRUE(ParseStatFile(OpenTestStatFile("temp1_input").get(),
                            ArcSystemStatCollector::kOneValueColumns,
                            &cpu_temp));
  EXPECT_EQ(52000, cpu_temp);
}

TEST_F(ArcSystemStatCollectorTest, Serialize) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath path =
      base_path.Append("arc_overview_tracing").Append("system_stat_collector");
  std::string json_content;
  ASSERT_TRUE(base::ReadFileToString(path, &json_content));
  ArcSystemStatCollector collector;
  ASSERT_TRUE(collector.LoadFromJson(json_content));
  const std::string json_content_restored = collector.SerializeToJson();
  ASSERT_TRUE(!json_content_restored.empty());
  std::optional<base::Value> root = base::JSONReader::Read(json_content);
  ASSERT_TRUE(root);
  std::optional<base::Value> root_restored =
      base::JSONReader::Read(json_content_restored);
  ASSERT_TRUE(root_restored);
  EXPECT_EQ(*root, *root_restored);
}

}  // namespace arc
