// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_FUZZTEST_SUPPORT_H_
#define BASE_TEST_FUZZTEST_SUPPORT_H_

#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace {

template <typename T>
  requires std::copy_constructible<T>
std::optional<std::tuple<T>> Wrap(base::optional_ref<const T> maybe_value) {
  return maybe_value.has_value() ? std::optional(std::tuple<T>{*maybe_value})
                                 : std::nullopt;
}

auto ArbitraryValueNull() {
  return fuzztest::ReversibleMap(
      [] { return base::Value(); },
      [](const base::Value& value) { return std::optional<std::tuple<>>{}; });
}

auto ArbitraryValueBool() {
  return fuzztest::ReversibleMap(
      [](bool boolean) { return base::Value(boolean); },
      [](const base::Value& value) { return Wrap<bool>(value.GetIfBool()); },
      fuzztest::Arbitrary<bool>());
}

auto ArbitraryValueInt() {
  return fuzztest::ReversibleMap(
      [](int number) { return base::Value(number); },
      [](const base::Value& value) { return Wrap<int>(value.GetIfInt()); },
      fuzztest::Arbitrary<int>());
}

auto ArbitraryValueDouble() {
  return fuzztest::ReversibleMap(
      [](double number) { return base::Value(number); },
      [](const base::Value& value) {
        return Wrap<double>(value.GetIfDouble());
      },
      fuzztest::Finite<double>());
}

auto ArbitraryValueString() {
  return fuzztest::ReversibleMap(
      [](std::string string) { return base::Value(std::move(string)); },
      [](const base::Value& value) {
        return Wrap<std::string>(value.GetIfString());
      },
      fuzztest::Utf8String());
}

auto ArbitraryValueBlob() {
  return fuzztest::ReversibleMap(
      [](std::vector<uint8_t> blob) { return base::Value(std::move(blob)); },
      [](const base::Value& value) {
        return Wrap<std::vector<uint8_t>>(value.GetIfBlob());
      },
      fuzztest::Arbitrary<std::vector<uint8_t>>());
}

auto ArbitraryList(fuzztest::Domain<base::Value> entry_domain) {
  return fuzztest::ReversibleMap(
      [](std::vector<base::Value> values) {
        auto list = base::ListValue::with_capacity(values.size());
        for (auto& value : values) {
          list.Append(std::move(value));
        }
        return list;
      },
      [](const base::ListValue& list) {
        return std::optional{
            std::tuple{base::ToVector(list, &base::Value::Clone)}};
      },
      fuzztest::ContainerOf<std::vector<base::Value>>(entry_domain));
}

auto ArbitraryValueList(fuzztest::Domain<base::Value> entry_domain) {
  return fuzztest::ReversibleMap(
      [](base::ListValue list) { return base::Value(std::move(list)); },
      [](const base::Value& value) {
        const auto* list = value.GetIfList();
        return list ? std::optional{std::tuple{list->Clone()}} : std::nullopt;
      },
      ArbitraryList(entry_domain));
}

auto ArbitraryDict(fuzztest::Domain<base::Value> value_domain) {
  return fuzztest::ReversibleMap(
      [](std::vector<std::pair<std::string, base::Value>> e) {
        return base::DictValue(std::make_move_iterator(e.begin()),
                               std::make_move_iterator(e.end()));
      },
      [](const base::DictValue& dict) {
        return std::optional{
            std::tuple{base::ToVector(dict, [](const auto& entry) {
              return std::make_pair(entry.first, entry.second.Clone());
            })}};
      },
      fuzztest::ContainerOf<std::vector<std::pair<std::string, base::Value>>>(
          fuzztest::PairOf(fuzztest::Utf8String(), value_domain)));
}

auto ArbitraryValueDict(fuzztest::Domain<base::Value> value_domain) {
  return fuzztest::ReversibleMap(
      [](base::DictValue dict) { return base::Value(std::move(dict)); },
      [](const base::Value& value) {
        const auto* dict = value.GetIfDict();
        return dict ? std::optional{std::tuple{dict->Clone()}} : std::nullopt;
      },
      ArbitraryDict(value_domain));
}

fuzztest::Domain<base::Value> ArbitraryValue() {
  fuzztest::DomainBuilder builder;
  builder.Set<base::Value>(
      "value",
      fuzztest::OneOf(ArbitraryValueNull(), ArbitraryValueBool(),
                      ArbitraryValueInt(), ArbitraryValueDouble(),
                      ArbitraryValueString(), ArbitraryValueBlob(),
                      ArbitraryValueList(builder.Get<base::Value>("value")),
                      ArbitraryValueDict(builder.Get<base::Value>("value"))));
  return std::move(builder).Finalize<base::Value>("value");
}

}  // namespace

template <>
class fuzztest::internal::ArbitraryImpl<base::Value>
    : public fuzztest::Domain<base::Value> {
 public:
  ArbitraryImpl() : fuzztest::Domain<base::Value>(ArbitraryValue()) {}
};

template <>
class fuzztest::internal::ArbitraryImpl<base::DictValue>
    : public fuzztest::Domain<base::DictValue> {
 public:
  ArbitraryImpl()
      : fuzztest::Domain<base::DictValue>(ArbitraryDict(ArbitraryValue())) {}
};

template <>
class fuzztest::internal::ArbitraryImpl<base::ListValue>
    : public fuzztest::Domain<base::ListValue> {
 public:
  ArbitraryImpl()
      : fuzztest::Domain<base::ListValue>(ArbitraryList(ArbitraryValue())) {}
};

#endif  // BASE_TEST_FUZZTEST_SUPPORT_H_
