// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/software_config.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace crostini {

TEST(SoftwareConfigTest, ParserWorksOnCorrectData) {
  // Empty config should be considered valid.
  EXPECT_TRUE(SoftwareConfig::FromJson("").has_value());

  const std::string valid_input =
      R"({
        "version": 1,
        "keys": [
          {
            "url": "https://example.com/apt/gpgkey"
          },
          {
            "url": "https://foobar.de/key.asc"
          }
        ],
        "sources": [
          {
            "line": "deb https://test.io/BestEditor/software/any/ any main"
          },
          {
            "line": "deb-src https://foo.bar/repo/src/ yummy contrib"
          }
        ],
        "packages": [
          {
            "name": "foo"
          },
          {
            "name": "foo-tools"
          },
          {
            "name": "libbf5bazserver5"
          }
        ]
      })";

  const auto config = SoftwareConfig::FromJson(valid_input);

  EXPECT_TRUE(config.has_value());
  EXPECT_THAT(config.value().key_urls(),
              testing::ElementsAre("https://example.com/apt/gpgkey",
                                   "https://foobar.de/key.asc"));
  EXPECT_THAT(config.value().source_lines(),
              testing::ElementsAre(
                  "deb https://test.io/BestEditor/software/any/ any main",
                  "deb-src https://foo.bar/repo/src/ yummy contrib"));
  EXPECT_THAT(config.value().package_names(),
              testing::ElementsAre("foo", "foo-tools", "libbf5bazserver5"));
}

TEST(SoftwareConfigTest, ParserFailsOnIncorrectData) {
  // Not a dictionary.
  EXPECT_FALSE(SoftwareConfig::FromJson("42").has_value());
  // No fields.
  EXPECT_FALSE(SoftwareConfig::FromJson("{}").has_value());
  // No version field.
  EXPECT_FALSE(SoftwareConfig::FromJson(
                   R"({
                        "keys": [],
                        "sources": [],
                        "packages": []
                      })")
                   .has_value());
  // No keys field.
  EXPECT_FALSE(SoftwareConfig::FromJson(
                   R"({
                        "version": 1,
                        "sources": [],
                        "packages": []
                      })")
                   .has_value());
  // No sources field.
  EXPECT_FALSE(SoftwareConfig::FromJson(
                   R"({
                       "version": 1,
                       "keys": [],
                       "packages": []
                      })")
                   .has_value());
  // No packages field.
  EXPECT_FALSE(SoftwareConfig::FromJson(
                   R"({
                        "version": 1,
                        "keys": [],
                        "sources": []
                      })")
                   .has_value());
  // Malformed JSON.
  EXPECT_FALSE(SoftwareConfig::FromJson(
                   R"({
                        version: 1
                        keys: []
                        sources: []
                        packages: []
                      })")
                   .has_value());
}

}  // namespace crostini
