// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contacts_sorter.h"

#include <algorithm>
#include <random>
#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

const std::vector<nearby::sharing::proto::ContactRecord>& contacts() {
  static const base::NoDestructor<
      std::vector<nearby::sharing::proto::ContactRecord>>
      contacts([] {
        nearby::sharing::proto::ContactRecord contact0;
        contact0.set_person_name("Claire");
        contact0.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact1;
        contact1.set_person_name("Alice");
        contact1.add_identifiers()->set_account_name("y@gmail.com");
        contact1.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact2;
        contact2.set_person_name("Alice");
        contact2.add_identifiers()->set_account_name("x@gmail.com");
        contact2.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact3;
        contact3.add_identifiers()->set_account_name("bob@gmail.com");
        contact3.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact4;
        contact4.add_identifiers()->set_phone_number("222-222-2222");
        contact4.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact5;
        contact5.add_identifiers()->set_phone_number("111-111-1111");
        contact5.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact6;
        contact6.set_person_name("David");
        contact6.add_identifiers()->set_account_name("z@gmail.com");
        contact6.add_identifiers()->set_phone_number("222-222-2222");
        contact6.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact7;
        contact7.set_person_name("David");
        contact7.add_identifiers()->set_account_name("z@gmail.com");
        contact7.add_identifiers()->set_phone_number("111-111-1111");
        contact7.set_id("2");
        contact7.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact8;
        contact8.set_person_name("David");
        contact8.add_identifiers()->set_account_name("z@gmail.com");
        contact8.add_identifiers()->set_phone_number("111-111-1111");
        contact8.set_id("1");
        contact8.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact9;
        contact9.set_person_name("中村光");
        contact9.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact10;
        contact10.set_person_name("王皓");
        contact10.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact11;
        contact11.set_person_name("中村俊輔");
        contact11.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact12;
        contact12.set_person_name("丁立人");
        contact12.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact13;
        contact13.set_person_name("Á");
        contact13.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact14;
        contact14.set_person_name("Ñ");
        contact14.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact15;
        contact15.set_person_name("å");
        contact15.set_id("5");
        contact15.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact16;
        contact16.set_person_name("Å");
        contact16.set_id("3");
        contact16.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact17;
        contact17.set_person_name("åz");
        contact17.set_id("4");
        contact17.set_is_reachable(true);

        nearby::sharing::proto::ContactRecord contact18;
        contact18.set_person_name("Opus");
        contact18.set_is_reachable(true);

        return std::vector<nearby::sharing::proto::ContactRecord>{
            contact0,  contact1,  contact2,  contact3,  contact4,
            contact5,  contact6,  contact7,  contact8,  contact9,
            contact10, contact11, contact12, contact13, contact14,
            contact15, contact16, contact17, contact18};
      }());
  return *contacts;
}

void VerifySort(
    const std::vector<nearby::sharing::proto::ContactRecord>& expected_contacts,
    const std::vector<nearby::sharing::proto::ContactRecord>& unsorted_contacts,
    icu::Locale locale) {
  // Try a few different permutations of |unsorted_contacts|, which should all
  // be sorted to |expected_contacts|.
  auto rng = std::default_random_engine{};
  for (size_t i = 0; i < 10u; ++i) {
    std::vector<nearby::sharing::proto::ContactRecord> sorted_contacts =
        contacts();
    std::shuffle(sorted_contacts.begin(), sorted_contacts.end(), rng);
    SortNearbyShareContactRecords(&sorted_contacts, locale);

    ASSERT_EQ(expected_contacts.size(), sorted_contacts.size());
    for (size_t j = 0; j < expected_contacts.size(); ++j) {
      EXPECT_EQ(expected_contacts[j].SerializeAsString(),
                sorted_contacts[j].SerializeAsString());
    }
  }
}

}  // namespace

TEST(NearbyShareContactsSorter, US) {
  // Expected ordering:
  //  Á        |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //  åz       |               |              | ID: 4
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //  丁立人   |               |              |
  //  中村俊輔 |               |              |
  //  中村光   |               |              |
  //  王皓     |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<nearby::sharing::proto::ContactRecord> expected_contacts{
      contacts()[13], contacts()[15], contacts()[16], contacts()[2],
      contacts()[1],  contacts()[17], contacts()[3],  contacts()[0],
      contacts()[8],  contacts()[7],  contacts()[6],  contacts()[14],
      contacts()[18], contacts()[12], contacts()[11], contacts()[9],
      contacts()[10], contacts()[5],  contacts()[4]};
  VerifySort(expected_contacts, contacts(), icu::Locale::getUS());
}

TEST(NearbyShareContactsSorter, Sweden) {
  // Expected ordering:
  //  Á        |               |              |
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  åz       |               |              | ID: 4
  //  丁立人   |               |              |
  //  中村俊輔 |               |              |
  //  中村光   |               |              |
  //  王皓     |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<nearby::sharing::proto::ContactRecord> expected_contacts{
      contacts()[13], contacts()[2],  contacts()[1],  contacts()[3],
      contacts()[0],  contacts()[8],  contacts()[7],  contacts()[6],
      contacts()[14], contacts()[18], contacts()[15], contacts()[16],
      contacts()[17], contacts()[12], contacts()[11], contacts()[9],
      contacts()[10], contacts()[5],  contacts()[4]};
  VerifySort(expected_contacts, contacts(),
             icu::Locale("SV", "SV", "Traditional_POSIX"));
}

TEST(NearbyShareContactsSorter, China) {
  // Expected ordering:
  //  丁立人   |               |              |
  //  王皓     |               |              |
  //  中村光   |               |              |
  //  中村俊輔 |               |              |
  //  Á        |               |              |
  //  å        |               |              | ID: 5
  //  Å        |               |              | ID: 3
  //  Alice    | x@gmail.com   |              |
  //  Alice    | y@gmail.com   |              |
  //  åz       |               |              | ID: 4
  //           | bob@gmail.com |              |
  //  Claire   |               |              |
  //  David    | z@gmail.com   | 111-111-1111 | ID: 1
  //  David    | z@gmail.com   | 111-111-1111 | ID: 2
  //  David    | z@gmail.com   | 222-222-2222 |
  //  Ñ        |               |              |
  //  Opus     |               |              |
  //           |               | 111-111-1111 |
  //           |               | 222-222-2222 |
  std::vector<nearby::sharing::proto::ContactRecord> expected_contacts{
      contacts()[12], contacts()[10], contacts()[9],  contacts()[11],
      contacts()[13], contacts()[15], contacts()[16], contacts()[2],
      contacts()[1],  contacts()[17], contacts()[3],  contacts()[0],
      contacts()[8],  contacts()[7],  contacts()[6],  contacts()[14],
      contacts()[18], contacts()[5],  contacts()[4]};
  VerifySort(expected_contacts, contacts(), icu::Locale::getChina());
}
