// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PROTOCOL_STRING_H_
#define CHROME_BROWSER_DEVTOOLS_PROTOCOL_STRING_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"

namespace base {
class Value;
}

namespace protocol {

class Value;

using String = std::string;

class StringBuilder {
 public:
  StringBuilder();
  ~StringBuilder();
  void append(const String&);
  void append(char);
  void append(const char*, size_t);
  String toString();
  void reserveCapacity(size_t);

 private:
  std::string string_;
};

class StringUtil {
 public:
  static String substring(const String& s, unsigned pos, unsigned len) {
    return s.substr(pos, len);
  }
  static String fromInteger(int number) { return base::IntToString(number); }
  static String fromDouble(double number) {
    String s = base::NumberToString(number);
    if (!s.empty() && s[0] == '.')
      s = "0" + s;
    return s;
  }
  static double toDouble(const char* s, size_t len, bool* ok) {
    double v = 0.0;
    *ok = base::StringToDouble(std::string(s, len), &v);
    return *ok ? v : 0.0;
  }
  static size_t find(const String& s, const char* needle) {
    return s.find(needle);
  }
  static size_t find(const String& s, const String& needle) {
    return s.find(needle);
  }
  static const size_t kNotFound = static_cast<size_t>(-1);
  static void builderAppend(StringBuilder& builder, const String& s) {
    builder.append(s);
  }
  static void builderAppend(StringBuilder& builder, char c) {
    builder.append(c);
  }
  static void builderAppend(StringBuilder& builder, const char* s, size_t len) {
    builder.append(s, len);
  }
  static void builderAppendQuotedString(StringBuilder& builder,
                                        const String& str);
  static void builderReserve(StringBuilder& builder, unsigned capacity) {
    builder.reserveCapacity(capacity);
  }
  static String builderToString(StringBuilder& builder) {
    return builder.toString();
  }
  static std::unique_ptr<protocol::Value> parseJSON(const String&);
};

// A read-only sequence of uninterpreted bytes with reference-counted storage.
// Though the templates for generating the protocol bindings reference
// this type, thus far it's not used in the Chrome layer, so we provide no
// implementation here and rely on the linker optimizing it away. If this
// changes, look to content/browser/devtools/protocol_string{.h,.cc} for
// inspiration.
class Binary {
 public:
  const uint8_t* data() const;
  size_t size() const;
  String toBase64() const;
  static Binary fromBase64(const String& base64, bool* success);
};

std::unique_ptr<protocol::Value> toProtocolValue(const base::Value* value,
                                                 int depth);
std::unique_ptr<base::Value> toBaseValue(protocol::Value* value, int depth);

}  // namespace protocol

#endif  // CHROME_BROWSER_DEVTOOLS_PROTOCOL_STRING_H_
