// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/keyboard_accessory/android/accessory_sheet_data.h"

#include "base/base64.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"

namespace autofill {

AccessorySheetField::AccessorySheetField() = default;

AccessorySheetField::AccessorySheetField(const AccessorySheetField&) = default;

AccessorySheetField::AccessorySheetField(AccessorySheetField&&) = default;

AccessorySheetField::~AccessorySheetField() = default;

AccessorySheetField& AccessorySheetField::operator=(
    const AccessorySheetField&) = default;

AccessorySheetField& AccessorySheetField::operator=(AccessorySheetField&&) =
    default;

std::ostream& operator<<(std::ostream& os, const AccessorySheetField& field) {
  os << "(display text: \"" << field.display_text() << "\", "
     << "text_to_fill: \"" << field.text_to_fill() << "\", "
     << "a11y_description: \"" << field.a11y_description() << "\", " << "id: \""
     << field.id() << "\", " << "icon_id: \"" << field.icon_id() << "\", "
     << "is " << (field.selectable() ? "" : "not ") << "selectable, " << "is "
     << (field.is_obfuscated() ? "" : "not ") << "obfuscated)";
  return os;
}

AccessorySheetField::Builder::Builder() = default;

AccessorySheetField::Builder::~Builder() = default;

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetDisplayText(
    std::u16string display_text) && {
  // Calls SetDisplayText(...)& since |this| is an lvalue.
  accessory_sheet_field_.set_display_text(std::move(display_text));
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetTextToFill(
    std::u16string text_to_fill) && {
  accessory_sheet_field_.set_text_to_fill(std::move(text_to_fill));
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetA11yDescription(
    std::u16string a11y_description) && {
  accessory_sheet_field_.set_a11y_description(std::move(a11y_description));
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetId(
    std::string id) && {
  accessory_sheet_field_.set_id(std::move(id));
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetIsObfuscated(
    bool is_obfuscated) && {
  accessory_sheet_field_.set_is_obfuscated(is_obfuscated);
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetSelectable(
    bool selectable) && {
  accessory_sheet_field_.set_selectable(selectable);
  return std::move(*this);
}

AccessorySheetField::Builder&& AccessorySheetField::Builder::SetIconId(
    int icon_id) && {
  accessory_sheet_field_.set_icon_id(icon_id);
  return std::move(*this);
}

AccessorySheetField&& AccessorySheetField::Builder::Build() && {
  if (accessory_sheet_field_.text_to_fill().empty()) {
    accessory_sheet_field_.set_text_to_fill(
        accessory_sheet_field_.display_text());
  }
  if (accessory_sheet_field_.a11y_description().empty()) {
    accessory_sheet_field_.set_a11y_description(
        accessory_sheet_field_.display_text());
  }
  return std::move(accessory_sheet_field_);
}

UserInfo::UserInfo() = default;

UserInfo::UserInfo(std::string origin)
    : UserInfo(std::move(origin), IsExactMatch(true)) {}

UserInfo::UserInfo(std::string origin, IsExactMatch is_exact_match)
    : UserInfo(std::move(origin), is_exact_match, GURL()) {}

UserInfo::UserInfo(std::string origin, GURL icon_url)
    : UserInfo(std::move(origin), IsExactMatch(true), std::move(icon_url)) {}

UserInfo::UserInfo(std::string origin,
                   IsExactMatch is_exact_match,
                   GURL icon_url)
    : origin_(std::move(origin)),
      is_exact_match_(is_exact_match),
      icon_url_(std::move(icon_url)) {}

UserInfo::UserInfo(const UserInfo&) = default;

UserInfo& UserInfo::operator=(const UserInfo&) = default;

UserInfo::UserInfo(UserInfo&&) = default;

UserInfo& UserInfo::operator=(UserInfo&&) = default;

UserInfo::~UserInfo() = default;

std::ostream& operator<<(std::ostream& os, const UserInfo& user_info) {
  os << "origin: \"" << user_info.origin() << "\", "
     << "is_exact_match: " << std::boolalpha << user_info.is_exact_match()
     << ", "
     << "icon_url: " << user_info.icon_url() << ","
     << "fields: [\n";
  for (const AccessorySheetField& field : user_info.fields()) {
    os << field << ", \n";
  }
  return os << "]";
}

UserInfoSection::UserInfoSection(std::u16string title)
    : title_(std::move(title)) {}

UserInfoSection::UserInfoSection(const UserInfoSection&) = default;

UserInfoSection& UserInfoSection::operator=(const UserInfoSection&) = default;

UserInfoSection::UserInfoSection(UserInfoSection&&) = default;

UserInfoSection& UserInfoSection::operator=(UserInfoSection&&) = default;

UserInfoSection::~UserInfoSection() = default;

std::ostream& operator<<(std::ostream& os, const UserInfoSection& section) {
  os << "with title: \"" << section.title() << "\" and user info list: [";
  for (const UserInfo& user_info : section.user_info_list()) {
    os << user_info << ", ";
  }
  os << "]";
  return os;
}

PlusAddressInfo::PlusAddressInfo(std::string origin,
                                 std::u16string plus_address)
    : origin_(std::move(origin)),
      plus_address_(AccessorySheetField::Builder()
                        .SetDisplayText(std::move(plus_address))
                        .SetSelectable(true)
                        .Build()) {}

PlusAddressInfo::PlusAddressInfo(const PlusAddressInfo&) = default;

PlusAddressInfo& PlusAddressInfo::operator=(const PlusAddressInfo&) = default;

PlusAddressInfo::PlusAddressInfo(PlusAddressInfo&&) = default;

PlusAddressInfo& PlusAddressInfo::operator=(PlusAddressInfo&&) = default;

PlusAddressInfo::~PlusAddressInfo() = default;

std::ostream& operator<<(std::ostream& os,
                         const PlusAddressInfo& plus_address) {
  os << "origin: \"" << plus_address.origin() << "\", " << "plus_address: \""
     << plus_address.plus_address().display_text() << "\"";
  return os;
}

PlusAddressSection::PlusAddressSection(std::u16string title)
    : title_(std::move(title)) {}

PlusAddressSection::PlusAddressSection(const PlusAddressSection&) = default;

PlusAddressSection& PlusAddressSection::operator=(const PlusAddressSection&) =
    default;

PlusAddressSection::PlusAddressSection(PlusAddressSection&&) = default;

PlusAddressSection& PlusAddressSection::operator=(PlusAddressSection&&) =
    default;

PlusAddressSection::~PlusAddressSection() = default;

std::ostream& operator<<(std::ostream& os,
                         const PlusAddressSection& plus_address_section) {
  os << "title: \"" << plus_address_section.title()
     << "\", plus address info list: [";
  for (const PlusAddressInfo& info :
       plus_address_section.plus_address_info_list()) {
    os << info << ", ";
  }
  os << "]";
  return os;
}

PasskeySection::PasskeySection(std::string display_name,
                               std::vector<uint8_t> passkey_id)
    : display_name_(std::move(display_name)),
      passkey_id_(std::move(passkey_id)) {}

PasskeySection::PasskeySection(const PasskeySection&) = default;

PasskeySection& PasskeySection::operator=(const PasskeySection&) = default;

PasskeySection::PasskeySection(PasskeySection&&) = default;

PasskeySection& PasskeySection::operator=(PasskeySection&&) = default;

PasskeySection::~PasskeySection() = default;

std::ostream& operator<<(std::ostream& os,
                         const PasskeySection& passkey_section) {
  os << "display_name: \"" << passkey_section.display_name() << "\", "
     << "passkey_id: \"" << base::Base64Encode(passkey_section.passkey_id())
     << "\"";
  return os;
}

PromoCodeInfo::PromoCodeInfo(std::u16string promo_code,
                             std::u16string details_text)
    : promo_code_(AccessorySheetField::Builder()
                      .SetDisplayText(std::move(promo_code))
                      .SetSelectable(true)
                      .Build()),
      details_text_(details_text) {}

PromoCodeInfo::PromoCodeInfo(const PromoCodeInfo&) = default;

PromoCodeInfo& PromoCodeInfo::operator=(const PromoCodeInfo&) = default;

PromoCodeInfo::PromoCodeInfo(PromoCodeInfo&&) = default;

PromoCodeInfo& PromoCodeInfo::operator=(PromoCodeInfo&&) = default;

PromoCodeInfo::~PromoCodeInfo() = default;

std::ostream& operator<<(std::ostream& os,
                         const PromoCodeInfo& promo_code_info) {
  os << "promo_code: \"" << promo_code_info.promo_code() << "\", "
     << "details_text: \"" << promo_code_info.details_text() << "\"";
  return os;
}

IbanInfo::IbanInfo(std::u16string value,
                   std::u16string text_to_fill,
                   std::string id)
    : value_(AccessorySheetField::Builder()
                 .SetDisplayText(std::move(value))
                 .SetTextToFill(std::move(text_to_fill))
                 .SetId(std::move(id))
                 .SetSelectable(true)
                 .Build()) {}

IbanInfo::IbanInfo(const IbanInfo&) = default;

IbanInfo& IbanInfo::operator=(const IbanInfo&) = default;

IbanInfo::IbanInfo(IbanInfo&&) = default;

IbanInfo& IbanInfo::operator=(IbanInfo&&) = default;

IbanInfo::~IbanInfo() = default;

std::ostream& operator<<(std::ostream& os, const IbanInfo& iban_info) {
  os << "iban_info: \"" << iban_info.value() << "\"";
  return os;
}

FooterCommand::FooterCommand(std::u16string display_text,
                             AccessoryAction action)
    : display_text_(std::move(display_text)), accessory_action_(action) {}

FooterCommand::FooterCommand(const FooterCommand&) = default;

FooterCommand& FooterCommand::operator=(const FooterCommand&) = default;

FooterCommand& FooterCommand::operator=(FooterCommand&&) = default;

FooterCommand::FooterCommand(FooterCommand&&) = default;

FooterCommand::~FooterCommand() = default;

std::ostream& operator<<(std::ostream& os, const FooterCommand& fc) {
  return os << "(display text: \"" << fc.display_text() << "\", "
            << "action: " << static_cast<int>(fc.accessory_action()) << ")";
}

OptionToggle::OptionToggle(std::u16string display_text,
                           bool enabled,
                           AccessoryAction action)
    : display_text_(display_text),
      enabled_(enabled),
      accessory_action_(action) {}

OptionToggle::OptionToggle(const OptionToggle&) = default;

OptionToggle& OptionToggle::operator=(const OptionToggle&) = default;

OptionToggle::OptionToggle(OptionToggle&&) = default;

OptionToggle& OptionToggle::operator=(OptionToggle&&) = default;

OptionToggle::~OptionToggle() = default;

std::ostream& operator<<(std::ostream& os, const OptionToggle& ot) {
  return os << "(display text: \"" << ot.display_text() << "\", "
            << "state: " << ot.is_enabled() << ", "
            << "action: " << static_cast<int>(ot.accessory_action()) << ")";
}

std::ostream& operator<<(std::ostream& os, const AccessoryTabType& type) {
  switch (type) {
    case AccessoryTabType::PASSWORDS:
      return os << "Passwords sheet";
    case AccessoryTabType::CREDIT_CARDS:
      return os << "Payments sheet";
    case AccessoryTabType::ADDRESSES:
      return os << "Address sheet";
    case AccessoryTabType::OBSOLETE_TOUCH_TO_FILL:
      return os << "(obsolete) Touch to Fill sheet";
    case AccessoryTabType::ALL:
      return os << "All sheets";
    case AccessoryTabType::COUNT:
      return os << "Invalid sheet";
  }
  return os;
}

AccessorySheetData::AccessorySheetData(AccessoryTabType sheet_type,
                                       std::u16string user_info_title,
                                       std::u16string plus_address_title)
    : AccessorySheetData(sheet_type,
                         std::move(user_info_title),
                         std::move(plus_address_title),
                         std::u16string()) {}

AccessorySheetData::AccessorySheetData(AccessoryTabType sheet_type,
                                       std::u16string user_info_title,
                                       std::u16string plus_address_title,
                                       std::u16string warning)
    : sheet_type_(sheet_type),
      warning_(std::move(warning)),
      plus_address_section_(std::move(plus_address_title)),
      user_info_section_(std::move(user_info_title)) {}

AccessorySheetData::AccessorySheetData(const AccessorySheetData&) = default;

AccessorySheetData& AccessorySheetData::operator=(const AccessorySheetData&) =
    default;

AccessorySheetData::AccessorySheetData(AccessorySheetData&&) = default;

AccessorySheetData& AccessorySheetData::operator=(AccessorySheetData&&) =
    default;

AccessorySheetData::~AccessorySheetData() = default;

std::ostream& operator<<(std::ostream& os, const AccessorySheetData& data) {
  os << data.get_sheet_type();
  if (data.option_toggle().has_value()) {
    os << "\", with option toggle: \"" << data.option_toggle().value();
  } else {
    os << "\", with option toggle: \"none";
  }

  os << "\", warning: \"" << data.warning() << "\", and passkey list: [";
  for (const PasskeySection& passkey_section : data.passkey_section_list()) {
    os << passkey_section << ", ";
  }
  os << "], and user info section " << data.user_info_section();
  os << ", and promo code info list: [";
  for (const PromoCodeInfo& promo_code_info : data.promo_code_info_list()) {
    os << promo_code_info << ", ";
  }
  os << "], and iban info list: [";
  for (const IbanInfo& iban_info : data.iban_info_list()) {
    os << iban_info << ", ";
  }
  os << "], and plus address section: " << data.plus_address_section();
  os << ", footer commands: [";
  for (const FooterCommand& footer_command : data.footer_commands()) {
    os << footer_command << ", ";
  }
  return os << "]";
}

AccessorySheetData::Builder::Builder(AccessoryTabType type,
                                     std::u16string user_info_title,
                                     std::u16string plus_address_title)
    : accessory_sheet_data_(type,
                            std::move(user_info_title),
                            std::move(plus_address_title)) {}

AccessorySheetData::Builder::~Builder() = default;

AccessorySheetData::Builder&& AccessorySheetData::Builder::SetWarning(
    std::u16string warning) && {
  // Calls SetWarning(std::u16string warning)()& since |this| is an lvalue.
  return std::move(SetWarning(std::move(warning)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::SetWarning(
    std::u16string warning) & {
  accessory_sheet_data_.set_warning(std::move(warning));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::SetOptionToggle(
    std::u16string display_text,
    bool enabled,
    AccessoryAction action) && {
  // Calls SetOptionToggle(...)& since |this| is an lvalue.
  return std::move(SetOptionToggle(std::move(display_text), enabled, action));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::SetOptionToggle(
    std::u16string display_text,
    bool enabled,
    AccessoryAction action) & {
  accessory_sheet_data_.set_option_toggle(
      OptionToggle(std::move(display_text), enabled, action));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddUserInfo(
    std::string origin,
    UserInfo::IsExactMatch is_exact_match,
    GURL icon_url) && {
  // Calls AddUserInfo()& since |this| is an lvalue.
  return std::move(
      AddUserInfo(std::move(origin), is_exact_match, std::move(icon_url)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddUserInfo(
    std::string origin,
    UserInfo::IsExactMatch is_exact_match,
    GURL icon_url) & {
  accessory_sheet_data_.add_user_info(
      UserInfo(std::move(origin), is_exact_match, std::move(icon_url)));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendSimpleField(
    std::u16string text) && {
  // Calls AppendSimpleField(...)& since |this| is an lvalue.
  return std::move(AppendSimpleField(std::move(text)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendSimpleField(
    std::u16string text) & {
  std::u16string display_text = text;
  std::u16string text_to_fill = text;
  std::u16string a11y_description = std::move(text);
  return AppendField(std::move(display_text), std::move(text_to_fill),
                     std::move(a11y_description), false, true);
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string a11y_description,
    bool is_obfuscated,
    bool selectable) && {
  std::u16string text_to_fill = display_text;
  // Calls AppendField(...)& since |this| is an lvalue.
  return std::move(AppendField(std::move(display_text), std::move(text_to_fill),
                               std::move(a11y_description), is_obfuscated,
                               selectable));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    bool is_obfuscated,
    bool selectable) & {
  accessory_sheet_data_.mutable_user_info_list().back().add_field(
      AccessorySheetField::Builder()
          .SetDisplayText(std::move(display_text))
          .SetTextToFill(std::move(text_to_fill))
          .SetA11yDescription(std::move(a11y_description))
          .SetIsObfuscated(is_obfuscated)
          .SetSelectable(selectable)
          .Build());
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    std::string id,
    bool is_obfuscated,
    bool selectable) && {
  // Calls AppendField(...)& since |this| is an lvalue.
  return std::move(AppendField(std::move(display_text), std::move(text_to_fill),
                               std::move(a11y_description), std::move(id),
                               is_obfuscated, selectable));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    std::string id,
    bool is_obfuscated,
    bool selectable) & {
  accessory_sheet_data_.mutable_user_info_list().back().add_field(
      AccessorySheetField::Builder()
          .SetDisplayText(std::move(display_text))
          .SetTextToFill(std::move(text_to_fill))
          .SetA11yDescription(std::move(a11y_description))
          .SetId(std::move(id))
          .SetIsObfuscated(is_obfuscated)
          .SetSelectable(selectable)
          .Build());
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendField(
    std::u16string display_text,
    std::u16string text_to_fill,
    std::u16string a11y_description,
    std::string id,
    int icon_id,
    bool is_obfuscated,
    bool selectable) && {
  accessory_sheet_data_.mutable_user_info_list().back().add_field(
      AccessorySheetField::Builder()
          .SetDisplayText(std::move(display_text))
          .SetTextToFill(std::move(text_to_fill))
          .SetA11yDescription(std::move(a11y_description))
          .SetId(std::move(id))
          .SetIconId(icon_id)
          .SetIsObfuscated(is_obfuscated)
          .SetSelectable(selectable)
          .Build());
  return std::move(*this);
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddPlusAddressInfo(
    std::string origin,
    std::u16string plus_address) && {
  // Calls AddPlusAddressInfo(...)& since |this| is an lvalue.
  return std::move(
      AddPlusAddressInfo(std::move(origin), std::move(plus_address)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddPlusAddressInfo(
    std::string origin,
    std::u16string plus_address) & {
  accessory_sheet_data_.add_plus_address_info(
      (PlusAddressInfo(std::move(origin), std::move(plus_address))));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddPasskeySection(
    std::string username,
    std::vector<uint8_t> credential_id) && {
  // Calls PasskeySection(...)& since |this| is an lvalue.
  return std::move(
      AddPasskeySection(std::move(username), std::move(credential_id)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddPasskeySection(
    std::string username,
    std::vector<uint8_t> credential_id) & {
  accessory_sheet_data_.add_passkey_section(
      (PasskeySection(std::move(username), std::move(credential_id))));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddPromoCodeInfo(
    std::u16string promo_code,
    std::u16string details_text) && {
  // Calls PromoCodeInfo(...)& since |this| is an lvalue.
  return std::move(
      AddPromoCodeInfo(std::move(promo_code), std::move(details_text)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddPromoCodeInfo(
    std::u16string promo_code,
    std::u16string details_text) & {
  accessory_sheet_data_.add_promo_code_info(
      (PromoCodeInfo(std::move(promo_code), std::move(details_text))));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AddIbanInfo(
    std::u16string value,
    std::u16string text_to_fill,
    std::string id) && {
  // Calls IbanInfo(...)& since `this` is an lvalue.
  return std::move(
      AddIbanInfo(std::move(value), std::move(text_to_fill), std::move(id)));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AddIbanInfo(
    std::u16string value,
    std::u16string text_to_fill,
    std::string id) & {
  accessory_sheet_data_.add_iban_info(
      (IbanInfo(std::move(value), std::move(text_to_fill), std::move(id))));
  return *this;
}

AccessorySheetData::Builder&& AccessorySheetData::Builder::AppendFooterCommand(
    std::u16string display_text,
    AccessoryAction action) && {
  // Calls AppendFooterCommand(...)& since |this| is an lvalue.
  return std::move(AppendFooterCommand(std::move(display_text), action));
}

AccessorySheetData::Builder& AccessorySheetData::Builder::AppendFooterCommand(
    std::u16string display_text,
    AccessoryAction action) & {
  accessory_sheet_data_.add_footer_command(
      FooterCommand(std::move(display_text), action));
  return *this;
}

AccessorySheetData&& AccessorySheetData::Builder::Build() && {
  return std::move(accessory_sheet_data_);
}

}  // namespace autofill
