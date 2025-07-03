// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/android_rsa.h"

#include <string>
#include <string_view>

#include "crypto/keypair.h"
#include "crypto/rsa_private_key.h"
#include "crypto/test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AndroidRSATest, EncodePublicKey) {
  auto key = crypto::test::FixedRsa2048PrivateKeyForTesting();
  auto rsa = crypto::RSAPrivateKey::CreateFromKey(key.key());
  ASSERT_TRUE(rsa);
  std::string_view expected =
      "QAAAAIuxyjLdkf9dwbd1cL9LyHJ2RPiBOob18Y8buAkZfERfSAQ2N7dTCc763pGJ2VyBj3kZ"
      "b7uhsG6KQz9RyY8dOJxX4ihd97sEEju6y277D2vsjP1WhSPHomOw19yxH3YKQOE0z6/3TeCF"
      "VVvW+7t2wc4LLF5xnrHvhGwI1YftkJcXU73OOLmHKOnd1uPFleO7IDj/Na41qsaEjoCFn4XY"
      "Z9NAUY6W8ws/ge8BV1gaD0QibRnBAylUjf6D43p3RlOo1QOubtI2YqDm+w3wopIS0LkesctP"
      "AsShaNyTndv3xd8gNCcHvTAupjZ3fNhd9dpxrFQ/wnBtACQXwYGrRd+4VkehEXO3LPo3yhS5"
      "wD0VAXGg/ah+fSICe+zEt8pb3Ivy26v1eprkO4/dPwe4Mb15HGOmKJxHtnIWThOZgb9/dUnc"
      "W9CQ1sldHAQuRAofrK2uXsVFksOu6oU8891WCpkoSdQOmi47thhTBQiC4v7ADUwz0EVnHpxD"
      "eGChiZTqv1xIZlL+LoDeymZJQ6xul3Ipgu94Z5WqqOUSu/nzhbzcbQMdiB727n4ZM+INSxmd"
      "F5l0GR6DzDb/nF52MZjPgNCHmjEa6vM/mkEjAFZVz7f4VfSg+MgWm9iO86YvfG+QyoZToSzt"
      "dZa5hBvzNQk/jJHYa7EGrCq1UOa4ctVeb6Nig29n3cgxfAEAAQA=";
  EXPECT_EQ(AndroidRSAPublicKey(rsa.get()), expected);

  // TODO(crbug.com/40283364): Also test that RSA keys that do not fit in
  // Android's custom RSA format fail cleanly. Right now they get very confused.
}
