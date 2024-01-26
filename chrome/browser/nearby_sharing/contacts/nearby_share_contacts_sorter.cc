// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/contacts/nearby_share_contacts_sorter.h"

#include <algorithm>
#include <optional>
#include <string>

#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/nearby/sharing/proto/rpc_resources.pb.h"

namespace {

struct ContactSortingFields {
  // Primary sorting key: person name if not empty; otherwise, email.
  std::optional<std::string> person_name_or_email;
  // Secondary sorting key. Note: It is okay if email is also used as the
  // primary sorting key.
  std::optional<std::string> email;
  // Tertiary sorting key.
  std::optional<std::string> phone_number;
  // Last resort sorting key. The contact ID should be unique for each contact
  // record, guaranteeing uniquely defined ordering.
  std::string id;
};

ContactSortingFields GetContactSortingFields(
    const nearby::sharing::proto::ContactRecord& contact) {
  ContactSortingFields fields;
  fields.id = contact.id();
  for (const auto& identifier : contact.identifiers()) {
    switch (identifier.identifier_case()) {
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kAccountName:
        if (!fields.email) {
          fields.email = identifier.account_name();
        }
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kPhoneNumber:
        if (!fields.phone_number) {
          fields.phone_number = identifier.phone_number();
        }
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          kObfuscatedGaia:
        break;
      case nearby::sharing::proto::Contact_Identifier::IdentifierCase::
          IDENTIFIER_NOT_SET:
        break;
    }
  }
  fields.person_name_or_email =
      contact.person_name().empty()
          ? fields.email
          : std::make_optional<std::string>(contact.person_name());

  return fields;
}

class ContactRecordComparator {
 public:
  explicit ContactRecordComparator(icu::Collator* collator)
      : collator_(collator) {}

  bool operator()(const nearby::sharing::proto::ContactRecord& c1,
                  const nearby::sharing::proto::ContactRecord& c2) const {
    ContactSortingFields f1 = GetContactSortingFields(c1);
    ContactSortingFields f2 = GetContactSortingFields(c2);

    switch (CollatorCompare(f1.person_name_or_email, f2.person_name_or_email)) {
      case UCOL_EQUAL:
        // Do nothing. Compare by next field.
        break;
      case UCOL_LESS:
        return true;
      case UCOL_GREATER:
        return false;
    }

    switch (CollatorCompare(f1.email, f2.email)) {
      case UCOL_EQUAL:
        // Do nothing. Compare by next field.
        break;
      case UCOL_LESS:
        return true;
      case UCOL_GREATER:
        return false;
    }

    if (f1.phone_number != f2.phone_number) {
      if (!f1.phone_number)
        return false;
      if (!f2.phone_number)
        return true;
      return *f1.phone_number < *f2.phone_number;
    }

    return f1.id < f2.id;
  }

 private:
  UCollationResult CollatorCompare(const std::optional<std::string>& a,
                                   const std::optional<std::string>& b) const {
    // Sort populated strings before std::nullopt.
    if (!a && !b)
      return UCOL_EQUAL;
    if (!b)
      return UCOL_LESS;
    if (!a)
      return UCOL_GREATER;

    // Sort using a locale-based collator if available.
    if (collator_) {
      return base::i18n::CompareString16WithCollator(
          *collator_, base::UTF8ToUTF16(*a), base::UTF8ToUTF16(*b));
    }

    // Fall back on standard string comparison, though we hope and expect that
    // locale-based sorting will succeed.
    if (*a == *b) {
      return UCOL_EQUAL;
    }
    return *a < *b ? UCOL_LESS : UCOL_GREATER;
  }

  raw_ptr<icu::Collator> collator_;
};

}  // namespace

void SortNearbyShareContactRecords(
    std::vector<nearby::sharing::proto::ContactRecord>* contacts,
    icu::Locale locale) {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  ContactRecordComparator comparator(U_SUCCESS(error) ? collator.get()
                                                      : nullptr);
  std::sort(contacts->begin(), contacts->end(), comparator);
}
