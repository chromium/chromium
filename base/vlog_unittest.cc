// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/vlog.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging {

namespace {

TEST(VlogTest, NoVmodule) {
  int min_log_level = 0;
  EXPECT_EQ(0,
            VlogInfo(std::string(), std::string(), &min_log_level)
                .GetVlogLevel("test1"));
  EXPECT_EQ(0,
            VlogInfo("0", std::string(), &min_log_level).GetVlogLevel("test2"));
  EXPECT_EQ(
      0, VlogInfo("blah", std::string(), &min_log_level).GetVlogLevel("test3"));
  EXPECT_EQ(
      0,
      VlogInfo("0blah1", std::string(), &min_log_level).GetVlogLevel("test4"));
  EXPECT_EQ(1,
            VlogInfo("1", std::string(), &min_log_level).GetVlogLevel("test5"));
  EXPECT_EQ(5,
            VlogInfo("5", std::string(), &min_log_level).GetVlogLevel("test6"));
}

TEST(VlogTest, MatchVlogPattern) {
  // Degenerate cases.
  EXPECT_TRUE(MatchVlogPattern("", ""));
  EXPECT_TRUE(MatchVlogPattern("", "****"));
  EXPECT_FALSE(MatchVlogPattern("", "x"));
  EXPECT_FALSE(MatchVlogPattern("x", ""));

  // Basic.
  EXPECT_TRUE(MatchVlogPattern("blah", "blah"));

  // ? should match exactly one character.
  EXPECT_TRUE(MatchVlogPattern("blah", "bl?h"));
  EXPECT_FALSE(MatchVlogPattern("blh", "bl?h"));
  EXPECT_FALSE(MatchVlogPattern("blaah", "bl?h"));
  EXPECT_TRUE(MatchVlogPattern("blah", "?lah"));
  EXPECT_FALSE(MatchVlogPattern("lah", "?lah"));
  EXPECT_FALSE(MatchVlogPattern("bblah", "?lah"));

  // * can match any number (even 0) of characters.
  EXPECT_TRUE(MatchVlogPattern("blah", "bl*h"));
  EXPECT_TRUE(MatchVlogPattern("blabcdefh", "bl*h"));
  EXPECT_TRUE(MatchVlogPattern("blh", "bl*h"));
  EXPECT_TRUE(MatchVlogPattern("blah", "*blah"));
  EXPECT_TRUE(MatchVlogPattern("ohblah", "*blah"));
  EXPECT_TRUE(MatchVlogPattern("blah", "blah*"));
  EXPECT_TRUE(MatchVlogPattern("blahhhh", "blah*"));
  EXPECT_TRUE(MatchVlogPattern("blahhhh", "blah*"));
  EXPECT_TRUE(MatchVlogPattern("blah", "*blah*"));
  EXPECT_TRUE(MatchVlogPattern("blahhhh", "*blah*"));
  EXPECT_TRUE(MatchVlogPattern("bbbblahhhh", "*blah*"));

  // Multiple *s should work fine.
  EXPECT_TRUE(MatchVlogPattern("ballaah", "b*la*h"));
  EXPECT_TRUE(MatchVlogPattern("blah", "b*la*h"));
  EXPECT_TRUE(MatchVlogPattern("bbbblah", "b*la*h"));
  EXPECT_TRUE(MatchVlogPattern("blaaah", "b*la*h"));

  // There should be no escaping going on.
  EXPECT_TRUE(MatchVlogPattern("bl\\ah", "bl\\?h"));
  EXPECT_FALSE(MatchVlogPattern("bl?h", "bl\\?h"));
  EXPECT_TRUE(MatchVlogPattern("bl\\aaaah", "bl\\*h"));
  EXPECT_FALSE(MatchVlogPattern("bl*h", "bl\\*h"));

  // Any slash matches any slash.
  EXPECT_TRUE(MatchVlogPattern("/b\\lah", "/b\\lah"));
  EXPECT_TRUE(MatchVlogPattern("\\b/lah", "/b\\lah"));
}

TEST(VlogTest, VmoduleBasic) {
  const char kVSwitch[] = "-1";
  const char kVModuleSwitch[] =
      "foo=,bar=0,baz=blah,,qux=0blah1,quux=1,corge.ext=5";
  int min_log_level = 0;
  VlogInfo vlog_info(kVSwitch, kVModuleSwitch, &min_log_level);
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("/path/to/grault.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/path/to/foo.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("D:\\Path\\To\\bar-inl.mm"));
  EXPECT_EQ(-1, vlog_info.GetVlogLevel("D:\\path\\to what/bar_unittest.m"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz.h"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/another/path/to/qux.h"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("/path/to/quux"));
  EXPECT_EQ(5, vlog_info.GetVlogLevel("c:\\path/to/corge.ext.h"));
}

TEST(VlogTest, VmoduleDirs) {
  const char kVModuleSwitch[] =
      "foo/bar.cc=1,baz\\*\\qux.cc=2,*quux/*=3,*/*-inl.h=4";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/foo/bar.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("bar.cc"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("foo/bar.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("baz/grault/qux.h"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("/baz/grault/qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz/grault/qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz/grault/blah/qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz\\grault\\qux.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("baz\\grault//blah\\qux.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("/foo/bar/baz/quux.cc"));
  EXPECT_EQ(3, vlog_info.GetVlogLevel("/foo/bar/baz/quux/grault.cc"));
  EXPECT_EQ(3, vlog_info.GetVlogLevel("/foo\\bar/baz\\quux/grault.cc"));

  EXPECT_EQ(0, vlog_info.GetVlogLevel("foo/bar/test-inl.cc"));
  EXPECT_EQ(4, vlog_info.GetVlogLevel("foo/bar/test-inl.h"));
  EXPECT_EQ(4, vlog_info.GetVlogLevel("foo/bar/baz/blah-inl.h"));
}

TEST(VlogTest, VmoduleDuplicateName) {
  // When filename rules are duplicated, the first one is effective.
  const char kVModuleSwitch[] = "foo=2,foo=1";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(2, vlog_info.GetVlogLevel("foo.cc"));
}

TEST(VlogTest, VmoduleDuplicatePattern) {
  // When pattern rules are duplicated, the first one is effective.
  const char kVModuleSwitch[] = "foo*=3,foo*=4";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(3, vlog_info.GetVlogLevel("foobar.cc"));
}

TEST(VlogTest, VmoduleOrderFirstMatchIsName) {
  // When rules overlap, the first matching rule is effective.
  // This is a filename before pattern case.
  const char kVModuleSwitch[] = "foo=2,bar/*=1";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(1, vlog_info.GetVlogLevel("bar/a.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("bar/foo.cc"));
}

TEST(VlogTest, VmoduleOrderFirstMatchIsPattern) {
  // When rules overlap, the first matching rule is effective.
  // This is a pattern before filename case.
  const char kVModuleSwitch[] = "bar/*=1,foo=2";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(1, vlog_info.GetVlogLevel("bar/foo.cc"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("bar/a.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("foo.cc"));
}

TEST(VlogTest, VmoduleOrderSample) {
  // logging.h --vmodule example, with some overlapping cases checked.
  const char kVModuleSwitch[] =
      "profile=2,icon_loader=1,browser_*=3,*/chromeos/*=4";
  int min_log_level = 0;
  VlogInfo vlog_info(std::string(), kVModuleSwitch, &min_log_level);
  EXPECT_EQ(4, vlog_info.GetVlogLevel("foo/chromeos/bar.cc"));
  EXPECT_EQ(3, vlog_info.GetVlogLevel("browser_foo.cc"));
  EXPECT_EQ(3, vlog_info.GetVlogLevel("foo/chromeos/browser_bar.cc"));
  EXPECT_EQ(1, vlog_info.GetVlogLevel("icon_loader.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("foo/profile.cc"));
  EXPECT_EQ(2, vlog_info.GetVlogLevel("foo/chromeos/profile.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("foo.cc"));
}

TEST(VlogTest, WithSwitches) {
  // Set up simple VlogInfo with just "foo".
  int min_log_level = 0;
  VlogInfo vlog_info("", "foo=1", &min_log_level);
  EXPECT_EQ(1, vlog_info.GetVlogLevel("foo.cc"));
  EXPECT_EQ(0, vlog_info.GetVlogLevel("bar.cc"));

  // Now create another one adding "bar".
  std::unique_ptr<VlogInfo> vlog_info_with_switches(
      vlog_info.WithSwitches("bar=2"));
  EXPECT_EQ(1, vlog_info_with_switches->GetVlogLevel("foo.cc"));
  EXPECT_EQ(2, vlog_info_with_switches->GetVlogLevel("bar.cc"));
}
}  // namespace

}  // namespace logging
