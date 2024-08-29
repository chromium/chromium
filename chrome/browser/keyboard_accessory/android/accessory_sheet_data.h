// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_DATA_H_
#define CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_DATA_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/types/strong_alias.h"
#include "chrome/browser/keyboard_accessory/android/accessory_sheet_enums.h"
#include "url/gurl.h"

namespace autofill {

// Represents a selectable item within a UserInfo or a PromoCodeInfo in the
// manual fallback UI, such as the username or a credit card number or a promo
// code.
class AccessorySheetField final {
 public:
  class Builder;

  AccessorySheetField(const AccessorySheetField&);
  AccessorySheetField& operator=(const AccessorySheetField&);
  AccessorySheetField(AccessorySheetField&&);
  AccessorySheetField& operator=(AccessorySheetField&&);

  ~AccessorySheetField();

  const std::u16string& display_text() const { return display_text_; }

  const std::u16string& text_to_fill() const { return text_to_fill_; }

  const std::u16string& a11y_description() const { return a11y_description_; }

  const std::string& id() const { return id_; }

  bool is_obfuscated() const { return is_obfuscated_; }

  bool selectable() const { return selectable_; }

  int icon_id() const { return icon_id_; }

  bool operator==(const AccessorySheetField&) const = default;

 private:
  friend class Builder;

  AccessorySheetField();

  void set_display_text(std::u16string display_text) {
    display_text_ = std::move(display_text);
  }

  void set_text_to_fill(std::u16string text_to_fill) {
    text_to_fill_ = std::move(text_to_fill);
  }

  void set_a11y_description(std::u16string a11y_description) {
    a11y_description_ = std::move(a11y_description);
  }

  void set_id(std::string id) { id_ = std::move(id); }

  void set_is_obfuscated(bool is_obfuscated) { is_obfuscated_ = is_obfuscated; }

  void set_selectable(bool selectable) { selectable_ = selectable; }

  void set_icon_id(int icon_id) { icon_id_ = icon_id; }

  std::u16string display_text_;
  // The string that would be used to fill in the form, for cases when it is
  // different from |display_text_|. For example: For unmasked credit cards,
  // the `display_text` contains spaces where as the `text_to_fill_` would
  // contain the card number without any spaces.
  std::u16string text_to_fill_;
  std::u16string a11y_description_;
  std::string id_;  // Optional, if needed to complete filling.
  int icon_id_ = 0;
  bool is_obfuscated_ = false;
  bool selectable_ = false;
};

class AccessorySheetField::Builder final {
 public:
  Builder();
  Builder(const Builder&) = default;
  Builder& operator=(const Builder&) = default;
  Builder(Builder&&) = default;
  Builder& operator=(Builder&&) = default;
  ~Builder();

  Builder&& SetDisplayText(std::u16string display_text) &&;

  Builder&& SetTextToFill(std::u16string text_to_fill) &&;

  Builder&& SetA11yDescription(std::u16string a11y_description) &&;

  Builder&& SetId(std::string id) &&;

  Builder&& SetIsObfuscated(bool is_obfuscated) &&;

  Builder&& SetSelectable(bool selectable) &&;

  Builder&& SetIconId(int icon_id) &&;

  // This class returns the constructed AccessorySheetField object. Since this
  // would render the builder unusable, it's required to destroy the object
  // afterwards. So if you hold the class in a variable, invoke like this:
  //   AccessorySheetField::Builder b;
  //   std::move(b).Build();
  AccessorySheetField&& Build() &&;

 private:
  AccessorySheetField accessory_sheet_field_;
};

// Represents user data to be shown on the manual fallback UI (e.g. a Profile,
// or a Credit Card, or the credentials for a website). For credentials,
// 'is_exact_match' is used to determine the origin (first-party match, a PSL or
// affiliated match) of the credential.
class UserInfo final {
 public:
  using IsExactMatch = base::StrongAlias<class IsExactMatchTag, bool>;

  UserInfo();
  explicit UserInfo(std::string origin);
  UserInfo(std::string origin, IsExactMatch is_exact_match);
  UserInfo(std::string origin, GURL icon_url);
  UserInfo(std::string origin, IsExactMatch is_exact_match, GURL icon_url);

  UserInfo(const UserInfo&);
  UserInfo& operator=(const UserInfo&);
  UserInfo(UserInfo&&);
  UserInfo& operator=(UserInfo&&);

  ~UserInfo();

  void add_field(AccessorySheetField field) {
    fields_.push_back(std::move(field));
  }

  const std::vector<AccessorySheetField>& fields() const { return fields_; }
  const std::string& origin() const { return origin_; }
  IsExactMatch is_exact_match() const { return is_exact_match_; }
  const GURL& icon_url() const { return icon_url_; }

  bool operator==(const UserInfo&) const = default;

 private:
  std::string origin_;
  // True means it's neither PSL match nor affiliated match, false otherwise.
  IsExactMatch is_exact_match_{true};
  std::vector<AccessorySheetField> fields_;
  GURL icon_url_;
};

std::ostream& operator<<(std::ostream& out, const AccessorySheetField& field);
std::ostream& operator<<(std::ostream& out, const UserInfo& user_info);

class UserInfoSection final {
 public:
  explicit UserInfoSection(std::u16string title);

  UserInfoSection(const UserInfoSection&);
  UserInfoSection& operator=(const UserInfoSection&);
  UserInfoSection(UserInfoSection&&);
  UserInfoSection& operator=(UserInfoSection&&);

  ~UserInfoSection();

  const std::u16string& title() const { return title_; }

  void add_user_info(UserInfo user_info) {
    user_info_list_.emplace_back(std::move(user_info));
  }

  const std::vector<UserInfo>& user_info_list() const {
    return user_info_list_;
  }

  std::vector<UserInfo>& mutable_user_info_list() { return user_info_list_; }

  bool operator==(const UserInfoSection&) const = default;

 private:
  std::u16string title_;
  std::vector<UserInfo> user_info_list_;
};

std::ostream& operator<<(std::ostream& os, const UserInfoSection& field);

class PlusAddressInfo final {
 public:
  PlusAddressInfo(std::string origin, std::u16string plus_address);

  PlusAddressInfo(const PlusAddressInfo&);
  PlusAddressInfo& operator=(const PlusAddressInfo&);
  PlusAddressInfo(PlusAddressInfo&&);
  PlusAddressInfo& operator=(PlusAddressInfo&&);

  ~PlusAddressInfo();

  const std::string& origin() const { return origin_; }
  const AccessorySheetField& plus_address() const { return plus_address_; }

  bool operator==(const PlusAddressInfo&) const = default;

 private:
  std::string origin_;
  AccessorySheetField plus_address_;
};

std::ostream& operator<<(std::ostream& out, const PlusAddressInfo& field);

class PlusAddressSection final {
 public:
  explicit PlusAddressSection(std::u16string title);

  PlusAddressSection(const PlusAddressSection&);
  PlusAddressSection& operator=(const PlusAddressSection&);
  PlusAddressSection(PlusAddressSection&&);
  PlusAddressSection& operator=(PlusAddressSection&&);

  ~PlusAddressSection();

  const std::u16string& title() const { return title_; }
  const std::vector<PlusAddressInfo>& plus_address_info_list() const {
    return plus_address_info_list_;
  }

  void add_plus_address_info(PlusAddressInfo info) {
    plus_address_info_list_.emplace_back(std::move(info));
  }

  bool operator==(const PlusAddressSection&) const = default;

 private:
  std::u16string title_;
  std::vector<PlusAddressInfo> plus_address_info_list_;
};

std::ostream& operator<<(std::ostream& out, const PlusAddressSection& field);

// Represents a passkey entry shown in the password accessory.
class PasskeySection final {
 public:
  PasskeySection(std::string display_name, std::vector<uint8_t> passkey_id);

  PasskeySection(const PasskeySection&);
  PasskeySection& operator=(const PasskeySection&);
  PasskeySection(PasskeySection&&);
  PasskeySection& operator=(PasskeySection&&);

  ~PasskeySection();

  const std::string& display_name() const { return display_name_; }

  const std::vector<uint8_t>& passkey_id() const { return passkey_id_; }

  bool operator==(const PasskeySection&) const = default;

 private:
  std::string display_name_;
  std::vector<uint8_t> passkey_id_;
};

std::ostream& operator<<(std::ostream& out,
                         const PasskeySection& passkey_section);

// Represents data pertaining to promo code offers to be shown on the Payments
// tab of manual fallback UI.
class PromoCodeInfo final {
 public:
  PromoCodeInfo(std::u16string promo_code, std::u16string details_text);

  PromoCodeInfo(const PromoCodeInfo&);
  PromoCodeInfo& operator=(const PromoCodeInfo&);
  PromoCodeInfo(PromoCodeInfo&&);
  PromoCodeInfo& operator=(PromoCodeInfo&&);

  ~PromoCodeInfo();

  const AccessorySheetField& promo_code() const { return promo_code_; }

  const std::u16string& details_text() const { return details_text_; }

  bool operator==(const PromoCodeInfo&) const = default;

 private:
  AccessorySheetField promo_code_;
  std::u16string details_text_;
};

std::ostream& operator<<(std::ostream& out,
                         const PromoCodeInfo& promo_code_info);

// Represents data pertaining to IBANs to be shown on the Payments methods
// tab of manual fallback UI.
class IbanInfo final {
 public:
  IbanInfo(std::u16string value, std::u16string text_to_fill, std::string id);

  IbanInfo(const IbanInfo&);
  IbanInfo& operator=(const IbanInfo&);
  IbanInfo(IbanInfo&&);
  IbanInfo& operator=(IbanInfo&&);

  ~IbanInfo();

  const AccessorySheetField& value() const { return value_; }

  bool operator==(const IbanInfo&) const = default;

 private:
  AccessorySheetField value_;
};

std::ostream& operator<<(std::ostream& out, const IbanInfo& iban);

// Represents a command below the suggestions, such as "Manage password...".
class FooterCommand final {
 public:
  FooterCommand(std::u16string display_text, AccessoryAction action);

  FooterCommand(const FooterCommand&);
  FooterCommand& operator=(const FooterCommand&);
  FooterCommand(FooterCommand&&);
  FooterCommand& operator=(FooterCommand&&);

  ~FooterCommand();

  const std::u16string& display_text() const { return display_text_; }

  AccessoryAction accessory_action() const { return accessory_action_; }

  bool operator==(const FooterCommand&) const = default;

 private:
  std::u16string display_text_;
  AccessoryAction accessory_action_;
};

std::ostream& operator<<(std::ostream& out, const FooterCommand& fc);

std::ostream& operator<<(std::ostream& out, const AccessoryTabType& type);

// Toggle to be displayed above the suggestions. One such toggle can be used,
// for example, to turn password saving on for the current origin.
class OptionToggle final {
 public:
  OptionToggle(std::u16string display_text,
               bool enabled,
               AccessoryAction accessory_action);

  OptionToggle(const OptionToggle&);
  OptionToggle& operator=(const OptionToggle&);
  OptionToggle(OptionToggle&&);
  OptionToggle& operator=(OptionToggle&&);

  ~OptionToggle();

  const std::u16string& display_text() const { return display_text_; }

  bool is_enabled() const { return enabled_; }

  AccessoryAction accessory_action() const { return accessory_action_; }

  bool operator==(const OptionToggle&) const = default;

 private:
  std::u16string display_text_;
  bool enabled_;
  AccessoryAction accessory_action_;
};

// Represents the contents of a bottom sheet tab below the keyboard accessory,
// which can correspond to passwords, credit cards, or profiles data.
class AccessorySheetData final {
 public:
  class Builder;

  AccessorySheetData(AccessoryTabType sheet_type,
                     std::u16string user_info_title,
                     std::u16string plus_address_title);
  AccessorySheetData(AccessoryTabType sheet_type,
                     std::u16string user_info_title,
                     std::u16string plus_address_title,
                     std::u16string warning);

  AccessorySheetData(const AccessorySheetData&);
  AccessorySheetData& operator=(const AccessorySheetData&);
  AccessorySheetData(AccessorySheetData&&);
  AccessorySheetData& operator=(AccessorySheetData&&);

  ~AccessorySheetData();

  const std::u16string& user_info_title() const {
    return user_info_section_.title();
  }
  AccessoryTabType get_sheet_type() const { return sheet_type_; }

  const std::u16string& warning() const { return warning_; }
  void set_warning(std::u16string warning) { warning_ = std::move(warning); }

  void set_option_toggle(OptionToggle toggle) {
    option_toggle_ = std::move(toggle);
  }
  const std::optional<OptionToggle>& option_toggle() const {
    return option_toggle_;
  }

  void add_user_info(UserInfo user_info) {
    user_info_section_.add_user_info(std::move(user_info));
  }

  void add_plus_address_info(PlusAddressInfo plus_address_info) {
    plus_address_section_.add_plus_address_info(std::move(plus_address_info));
  }

  void add_passkey_section(PasskeySection passkey_section) {
    passkey_section_list_.emplace_back(std::move(passkey_section));
  }

  const UserInfoSection& user_info_section() const {
    return user_info_section_;
  }

  const std::vector<UserInfo>& user_info_list() const {
    return user_info_section_.user_info_list();
  }

  const PlusAddressSection& plus_address_section() const {
    return plus_address_section_;
  }

  const std::u16string plus_address_title() const {
    return plus_address_section_.title();
  }

  const std::vector<PlusAddressInfo>& plus_address_info_list() const {
    return plus_address_section_.plus_address_info_list();
  }

  const std::vector<PasskeySection>& passkey_section_list() const {
    return passkey_section_list_;
  }

  std::vector<UserInfo>& mutable_user_info_list() {
    return user_info_section_.mutable_user_info_list();
  }

  void add_promo_code_info(PromoCodeInfo promo_code_info) {
    promo_code_info_list_.emplace_back(std::move(promo_code_info));
  }

  const std::vector<PromoCodeInfo>& promo_code_info_list() const {
    return promo_code_info_list_;
  }

  void add_iban_info(IbanInfo iban_info) {
    iban_info_list_.emplace_back(std::move(iban_info));
  }

  const std::vector<IbanInfo>& iban_info_list() const {
    return iban_info_list_;
  }

  void add_footer_command(FooterCommand footer_command) {
    footer_commands_.emplace_back(std::move(footer_command));
  }

  const std::vector<FooterCommand>& footer_commands() const {
    return footer_commands_;
  }

  bool operator==(const AccessorySheetData&) const = default;

 private:
  AccessoryTabType sheet_type_;
  std::u16string warning_;
  std::optional<OptionToggle> option_toggle_;
  PlusAddressSection plus_address_section_;
  std::vector<PasskeySection> passkey_section_list_;
  UserInfoSection user_info_section_;
  std::vector<PromoCodeInfo> promo_code_info_list_;
  std::vector<IbanInfo> iban_info_list_;
  std::vector<FooterCommand> footer_commands_;
};

std::ostream& operator<<(std::ostream& out, const AccessorySheetData& data);

// Helper class for AccessorySheetData objects creation.
//
// Example that creates a AccessorySheetData object with two UserInfo objects;
// the former has two fields, whereas the latter has three fields:
//   AccessorySheetData data = AccessorySheetData::Builder(user_info_title)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//       .AddUserInfo()
//           .AppendField(...)
//           .AppendField(...)
//           .AppendField(...)
//       .Build();
class AccessorySheetData::Builder final {
 public:
  Builder(AccessoryTabType type,
          std::u16string user_info_title,
          std::u16string plus_address_title);
  ~Builder();

  // Adds a warning string to the accessory sheet.
  Builder&& SetWarning(std::u16string warning) &&;
  Builder& SetWarning(std::u16string warning) &;

  // Sets the option toggle in the accessory sheet.
  Builder&& SetOptionToggle(std::u16string display_text,
                            bool enabled,
                            AccessoryAction action) &&;
  Builder& SetOptionToggle(std::u16string display_text,
                           bool enabled,
                           AccessoryAction action) &;

  // Adds a new UserInfo object to |accessory_sheet_data_|.
  Builder&& AddUserInfo(
      std::string origin = std::string(),
      UserInfo::IsExactMatch is_exact_match = UserInfo::IsExactMatch(true),
      GURL icon_url = GURL()) &&;
  Builder& AddUserInfo(
      std::string origin = std::string(),
      UserInfo::IsExactMatch is_exact_match = UserInfo::IsExactMatch(true),
      GURL icon_url = GURL()) &;

  // Appends a selectable, non-obfuscated field to the last UserInfo object.
  Builder&& AppendSimpleField(std::u16string text) &&;
  Builder& AppendSimpleField(std::u16string text) &;

  // Appends a field to the last UserInfo object.
  Builder&& AppendField(std::u16string display_text,
                        std::u16string a11y_description,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(std::u16string display_text,
                       std::u16string text_to_fill,
                       std::u16string a11y_description,
                       bool is_obfuscated,
                       bool selectable) &;

  Builder&& AppendField(std::u16string display_text,
                        std::u16string text_to_fill,
                        std::u16string a11y_description,
                        std::string id,
                        bool is_obfuscated,
                        bool selectable) &&;
  Builder& AppendField(std::u16string display_text,
                       std::u16string text_to_fill,
                       std::u16string a11y_description,
                       std::string id,
                       bool is_obfuscated,
                       bool selectable) &;

  Builder&& AppendField(std::u16string display_text,
                        std::u16string text_to_fill,
                        std::u16string a11y_description,
                        std::string id,
                        int icon_id,
                        bool is_obfuscated,
                        bool selectable) &&;

  // Adds a new PlusAddressInfo `accessory_sheet_data_`.
  Builder&& AddPlusAddressInfo(std::string origin,
                               std::u16string plus_address) &&;
  Builder& AddPlusAddressInfo(std::string origin,
                              std::u16string plus_address) &;

  // Adds a new PasskeySection `accessory_sheet_data_`.
  Builder&& AddPasskeySection(std::string username,
                              std::vector<uint8_t> credential_id) &&;
  Builder& AddPasskeySection(std::string username,
                             std::vector<uint8_t> credential_id) &;

  // Adds a new PromoCodeInfo object to |accessory_sheet_data_|.
  Builder&& AddPromoCodeInfo(std::u16string promo_code,
                             std::u16string details_text) &&;
  Builder& AddPromoCodeInfo(std::u16string promo_code,
                            std::u16string details_text) &;

  Builder&& AddIbanInfo(std::u16string value,
                        std::u16string text_to_fill,
                        std::string id) &&;
  Builder& AddIbanInfo(std::u16string value,
                       std::u16string text_to_fill,
                       std::string id) &;

  // Appends a new footer command to |accessory_sheet_data_|.
  Builder&& AppendFooterCommand(std::u16string display_text,
                                AccessoryAction action) &&;
  Builder& AppendFooterCommand(std::u16string display_text,
                               AccessoryAction action) &;

  // This class returns the constructed AccessorySheetData object. Since this
  // would render the builder unusable, it's required to destroy the object
  // afterwards. So if you hold the class in a variable, invoke like this:
  //   AccessorySheetData::Builder b(user_info_title);
  //   std::move(b).Build();
  AccessorySheetData&& Build() &&;

 private:
  AccessorySheetData accessory_sheet_data_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_KEYBOARD_ACCESSORY_ANDROID_ACCESSORY_SHEET_DATA_H_
