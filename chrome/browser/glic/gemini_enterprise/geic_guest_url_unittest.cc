// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/gemini_enterprise/geic_guest_url.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace geic {
namespace {

TEST(GeicGuestUrlTest, ConfigIdQuery) {
  // Extracts `configId` into `/home/cid/<configId>/r/skills`, strips
  // `configId`, `location`, `hl`, and `idp`, and preserves other query
  // parameters.
  EXPECT_EQ(
      GetGeicSkillsUrlForGuestUrl(GURL(
          "https://business.gemini.google/side-panel?"
          "configId=abc123&location=global&hl=en&mods=widget_staging_api_mod")),
      GURL("https://business.gemini.google/home/cid/abc123/r/skills?"
           "mods=widget_staging_api_mod"));
  EXPECT_EQ(
      GetGeicSkillsUrlForGuestUrl(
          GURL("https://vertexaisearch.cloud.google/side-panel?"
               "configId=abc123&idp=locations%2Fglobal%2FworkforcePools%"
               "2Fmy-pool-1%2Fproviders%2Fmy-idp")),
      GURL("https://vertexaisearch.cloud.google/home/cid/abc123/r/skills"));
}

TEST(GeicGuestUrlTest, CidInPath) {
  // Gemini at Work URLs carry the cid in the path: keep everything up to
  // `/cid/<configId>` and append `/customize/skills`.
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.google.com/work/cid/abc123/side-panel")),
            GURL("https://gemini.google.com/work/cid/abc123/customize/skills"));
  EXPECT_EQ(
      GetGeicSkillsUrlForGuestUrl(
          GURL("https://gemini.google.com/work/u/1/cid/abc123")),
      GURL("https://gemini.google.com/work/u/1/cid/abc123/customize/skills"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(GURL(
                "https://gemini.cloud.google/cid/abc123/side-panel?hl=en")),
            GURL("https://gemini.cloud.google/cid/abc123/customize/skills"));
  EXPECT_EQ(
      GetGeicSkillsUrlForGuestUrl(GURL(
          "https://gemini.cloud.google/work/us/cid/abc123/side-panel?mods=x")),
      GURL("https://gemini.cloud.google/work/us/cid/abc123/customize/"
           "skills?mods=x"));
}

TEST(GeicGuestUrlTest, MountRootWithoutCid) {
  // Gemini at Work paths without a cid resolve to the mount root (the server
  // redirects it to the user's instance).
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(GURL("https://gemini.google.com/work")),
            GURL("https://gemini.google.com/work"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.google.com/work/side-panel")),
            GURL("https://gemini.google.com/work"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.google.com/work/u/2/side-panel")),
            GURL("https://gemini.google.com/work/u/2"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.cloud.google/side-panel")),
            GURL("https://gemini.cloud.google/"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.cloud.google/work/side-panel")),
            GURL("https://gemini.cloud.google/work"));
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(
                GURL("https://gemini.cloud.google/u/1/side-panel")),
            GURL("https://gemini.cloud.google/u/1"));
}

TEST(GeicGuestUrlTest, OtherUrls) {
  // Other URLs without `configId` fall back to `/r/skills`.
  EXPECT_EQ(GetGeicSkillsUrlForGuestUrl(GURL(
                "https://business.gemini.google/side-panel?location=us&hl=fr")),
            GURL("https://business.gemini.google/r/skills"));
}

TEST(GeicGuestUrlTest, InvalidUrl) {
  // Returns an empty `GURL` when the resolved guest URL is empty/invalid.
  EXPECT_TRUE(GetGeicSkillsUrlForGuestUrl(GURL()).is_empty());
  EXPECT_TRUE(GetGeicSkillsUrlForGuestUrl(GURL("not a url")).is_empty());
}

}  // namespace
}  // namespace geic
