// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_piece.h"

#include <algorithm>
#include <climits>
#include <limits>
#include <ostream>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace base {
namespace {

// For each character in characters_wanted, sets the index corresponding
// to the ASCII code of that character to 1 in table.  This is used by
// the find_.*_of methods below to tell whether or not a character is in
// the lookup table in constant time.
// The argument `table' must be an array that is large enough to hold all
// the possible values of an unsigned char.  Thus it should be be declared
// as follows:
//   bool table[UCHAR_MAX + 1]
inline void BuildLookupTable(StringPiece characters_wanted, bool* table) {
  const size_t length = characters_wanted.length();
  const char* const data = characters_wanted.data();
  for (size_t i = 0; i < length; ++i) {
    table[static_cast<unsigned char>(data[i])] = true;
  }
}

}  // namespace

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
template class BasicStringPiece<char>;
template class BasicStringPiece<char16_t>;
template class BasicStringPiece<wchar_t>;
#endif

std::ostream& operator<<(std::ostream& o, StringPiece piece) {
  o.write(piece.data(), static_cast<std::streamsize>(piece.size()));
  return o;
}

std::ostream& operator<<(std::ostream& o, StringPiece16 piece) {
  return o << UTF16ToUTF8(piece);
}

std::ostream& operator<<(std::ostream& o, WStringPiece piece) {
  return o << WideToUTF8(piece);
}

namespace internal {

template <typename T, typename CharT = typename T::value_type>
size_t findT(T self, T s, size_t pos) {
  if (pos > self.size())
    return BasicStringPiece<CharT>::npos;

  typename BasicStringPiece<CharT>::const_iterator result =
      std::search(self.begin() + pos, self.end(), s.begin(), s.end());
  const size_t xpos =
    static_cast<size_t>(result - self.begin());
  return xpos + s.size() <= self.size() ? xpos : BasicStringPiece<CharT>::npos;
}

size_t find(StringPiece self, StringPiece s, size_t pos) {
  return findT(self, s, pos);
}

size_t find(StringPiece16 self, StringPiece16 s, size_t pos) {
  return findT(self, s, pos);
}

template <typename T, typename CharT = typename T::value_type>
size_t rfindT(T self, T s, size_t pos) {
  if (self.size() < s.size())
    return BasicStringPiece<CharT>::npos;

  if (s.empty())
    return std::min(self.size(), pos);

  typename BasicStringPiece<CharT>::const_iterator last =
      self.begin() + std::min(self.size() - s.size(), pos) + s.size();
  typename BasicStringPiece<CharT>::const_iterator result =
      std::find_end(self.begin(), last, s.begin(), s.end());
  return result != last ? static_cast<size_t>(result - self.begin())
                        : BasicStringPiece<CharT>::npos;
}

size_t rfind(StringPiece self, StringPiece s, size_t pos) {
  return rfindT(self, s, pos);
}

size_t rfind(StringPiece16 self, StringPiece16 s, size_t pos) {
  return rfindT(self, s, pos);
}

// 8-bit version using lookup table.
size_t find_first_of(StringPiece self, StringPiece s, size_t pos) {
  if (self.size() == 0 || s.size() == 0)
    return StringPiece::npos;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.size() == 1)
    return self.find(s.data()[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_t i = pos; i < self.size(); ++i) {
    if (lookup[static_cast<unsigned char>(self.data()[i])]) {
      return i;
    }
  }
  return StringPiece::npos;
}

// Generic brute force version.
template <typename T, typename CharT = typename T::value_type>
size_t find_first_ofT(T self, T s, size_t pos) {
  // Use the faster std::find() if searching for a single character.
  typename BasicStringPiece<CharT>::const_iterator found =
      s.size() == 1 ? std::find(self.begin() + pos, self.end(), s[0])
                    : std::find_first_of(self.begin() + pos, self.end(),
                                         s.begin(), s.end());
  if (found == self.end())
    return BasicStringPiece<CharT>::npos;
  return found - self.begin();
}

size_t find_first_of(StringPiece16 self, StringPiece16 s, size_t pos) {
  return find_first_ofT(self, s, pos);
}

// 8-bit version using lookup table.
size_t find_first_not_of(StringPiece self, StringPiece s, size_t pos) {
  if (pos >= self.size())
    return StringPiece::npos;

  if (s.size() == 0)
    return pos;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.size() == 1)
    return self.find_first_not_of(s.data()[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_t i = pos; i < self.size(); ++i) {
    if (!lookup[static_cast<unsigned char>(self.data()[i])]) {
      return i;
    }
  }
  return StringPiece::npos;
}

// Generic brute-force version.
template <typename T, typename CharT = typename T::value_type>
size_t find_first_not_ofT(T self, T s, size_t pos) {
  if (self.size() == 0)
    return BasicStringPiece<CharT>::npos;

  for (size_t self_i = pos; self_i < self.size(); ++self_i) {
    bool found = false;
    for (auto c : s) {
      if (self[self_i] == c) {
        found = true;
        break;
      }
    }
    if (!found)
      return self_i;
  }
  return BasicStringPiece<CharT>::npos;
}

size_t find_first_not_of(StringPiece16 self, StringPiece16 s, size_t pos) {
  return find_first_not_ofT(self, s, pos);
}

// 8-bit version using lookup table.
size_t find_last_of(StringPiece self, StringPiece s, size_t pos) {
  if (self.size() == 0 || s.size() == 0)
    return StringPiece::npos;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.size() == 1)
    return self.rfind(s.data()[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (size_t i = std::min(pos, self.size() - 1); ; --i) {
    if (lookup[static_cast<unsigned char>(self.data()[i])])
      return i;
    if (i == 0)
      break;
  }
  return StringPiece::npos;
}

// Generic brute-force version.
template <typename T, typename CharT = typename T::value_type>
size_t find_last_ofT(T self, T s, size_t pos) {
  if (self.size() == 0)
    return BasicStringPiece<CharT>::npos;

  for (size_t self_i = std::min(pos, self.size() - 1); ;
       --self_i) {
    for (auto c : s) {
      if (self.data()[self_i] == c)
        return self_i;
    }
    if (self_i == 0)
      break;
  }
  return BasicStringPiece<CharT>::npos;
}

size_t find_last_of(StringPiece16 self, StringPiece16 s, size_t pos) {
  return find_last_ofT(self, s, pos);
}

// 8-bit version using lookup table.
size_t find_last_not_of(StringPiece self, StringPiece s, size_t pos) {
  if (self.size() == 0)
    return StringPiece::npos;

  size_t i = std::min(pos, self.size() - 1);
  if (s.size() == 0)
    return i;

  // Avoid the cost of BuildLookupTable() for a single-character search.
  if (s.size() == 1)
    return self.find_last_not_of(s.data()[0], pos);

  bool lookup[UCHAR_MAX + 1] = { false };
  BuildLookupTable(s, lookup);
  for (; ; --i) {
    if (!lookup[static_cast<unsigned char>(self.data()[i])])
      return i;
    if (i == 0)
      break;
  }
  return StringPiece::npos;
}

// Generic brute-force version.
template <typename T, typename CharT = typename T::value_type>
size_t find_last_not_ofT(T self, T s, size_t pos) {
  if (self.size() == 0)
    return StringPiece::npos;

  for (size_t self_i = std::min(pos, self.size() - 1); ; --self_i) {
    bool found = false;
    for (auto c : s) {
      if (self.data()[self_i] == c) {
        found = true;
        break;
      }
    }
    if (!found)
      return self_i;
    if (self_i == 0)
      break;
  }
  return BasicStringPiece<CharT>::npos;
}

size_t find_last_not_of(StringPiece16 self, StringPiece16 s, size_t pos) {
  return find_last_not_ofT(self, s, pos);
}

size_t find(WStringPiece self, WStringPiece s, size_t pos) {
  return findT(self, s, pos);
}

size_t rfind(WStringPiece self, WStringPiece s, size_t pos) {
  return rfindT(self, s, pos);
}

size_t find_first_of(WStringPiece self, WStringPiece s, size_t pos) {
  return find_first_ofT(self, s, pos);
}

size_t find_first_not_of(WStringPiece self, WStringPiece s, size_t pos) {
  return find_first_not_ofT(self, s, pos);
}

size_t find_last_of(WStringPiece self, WStringPiece s, size_t pos) {
  return find_last_ofT(self, s, pos);
}

size_t find_last_not_of(WStringPiece self, WStringPiece s, size_t pos) {
  return find_last_not_ofT(self, s, pos);
}
}  // namespace internal
}  // namespace base
