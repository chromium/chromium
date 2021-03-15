// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resources/chromeos/zip_archiver/cpp/char_coding.h"

#include <string.h>
#include <string>

#include "base/strings/utf_string_conversions.h"

// Conversion table from code page 437 to Unicode.
// Based on ftp://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP437.TXT
const char16_t kCp437ToUnicodeTable[256] = {
    u'\u0000', u'\u0001', u'\u0002', u'\u0003', u'\u0004', u'\u0005', u'\u0006',
    u'\u0007', u'\u0008', u'\u0009', u'\u000a', u'\u000b', u'\u000c', u'\u000d',
    u'\u000e', u'\u000f', u'\u0010', u'\u0011', u'\u0012', u'\u0013', u'\u0014',
    u'\u0015', u'\u0016', u'\u0017', u'\u0018', u'\u0019', u'\u001a', u'\u001b',
    u'\u001c', u'\u001d', u'\u001e', u'\u001f', u'\u0020', u'\u0021', u'\u0022',
    u'\u0023', u'\u0024', u'\u0025', u'\u0026', u'\u0027', u'\u0028', u'\u0029',
    u'\u002a', u'\u002b', u'\u002c', u'\u002d', u'\u002e', u'\u002f', u'\u0030',
    u'\u0031', u'\u0032', u'\u0033', u'\u0034', u'\u0035', u'\u0036', u'\u0037',
    u'\u0038', u'\u0039', u'\u003a', u'\u003b', u'\u003c', u'\u003d', u'\u003e',
    u'\u003f', u'\u0040', u'\u0041', u'\u0042', u'\u0043', u'\u0044', u'\u0045',
    u'\u0046', u'\u0047', u'\u0048', u'\u0049', u'\u004a', u'\u004b', u'\u004c',
    u'\u004d', u'\u004e', u'\u004f', u'\u0050', u'\u0051', u'\u0052', u'\u0053',
    u'\u0054', u'\u0055', u'\u0056', u'\u0057', u'\u0058', u'\u0059', u'\u005a',
    u'\u005b', u'\u005c', u'\u005d', u'\u005e', u'\u005f', u'\u0060', u'\u0061',
    u'\u0062', u'\u0063', u'\u0064', u'\u0065', u'\u0066', u'\u0067', u'\u0068',
    u'\u0069', u'\u006a', u'\u006b', u'\u006c', u'\u006d', u'\u006e', u'\u006f',
    u'\u0070', u'\u0071', u'\u0072', u'\u0073', u'\u0074', u'\u0075', u'\u0076',
    u'\u0077', u'\u0078', u'\u0079', u'\u007a', u'\u007b', u'\u007c', u'\u007d',
    u'\u007e', u'\u007f', u'\u00c7', u'\u00fc', u'\u00e9', u'\u00e2', u'\u00e4',
    u'\u00e0', u'\u00e5', u'\u00e7', u'\u00ea', u'\u00eb', u'\u00e8', u'\u00ef',
    u'\u00ee', u'\u00ec', u'\u00c4', u'\u00c5', u'\u00c9', u'\u00e6', u'\u00c6',
    u'\u00f4', u'\u00f6', u'\u00f2', u'\u00fb', u'\u00f9', u'\u00ff', u'\u00d6',
    u'\u00dc', u'\u00a2', u'\u00a3', u'\u00a5', u'\u20a7', u'\u0192', u'\u00e1',
    u'\u00ed', u'\u00f3', u'\u00fa', u'\u00f1', u'\u00d1', u'\u00aa', u'\u00ba',
    u'\u00bf', u'\u2310', u'\u00ac', u'\u00bd', u'\u00bc', u'\u00a1', u'\u00ab',
    u'\u00bb', u'\u2591', u'\u2592', u'\u2593', u'\u2502', u'\u2524', u'\u2561',
    u'\u2562', u'\u2556', u'\u2555', u'\u2563', u'\u2551', u'\u2557', u'\u255d',
    u'\u255c', u'\u255b', u'\u2510', u'\u2514', u'\u2534', u'\u252c', u'\u251c',
    u'\u2500', u'\u253c', u'\u255e', u'\u255f', u'\u255a', u'\u2554', u'\u2569',
    u'\u2566', u'\u2560', u'\u2550', u'\u256c', u'\u2567', u'\u2568', u'\u2564',
    u'\u2565', u'\u2559', u'\u2558', u'\u2552', u'\u2553', u'\u256b', u'\u256a',
    u'\u2518', u'\u250c', u'\u2588', u'\u2584', u'\u258c', u'\u2590', u'\u2580',
    u'\u03b1', u'\u00df', u'\u0393', u'\u03c0', u'\u03a3', u'\u03c3', u'\u00b5',
    u'\u03c4', u'\u03a6', u'\u0398', u'\u03a9', u'\u03b4', u'\u221e', u'\u03c6',
    u'\u03b5', u'\u2229', u'\u2261', u'\u00b1', u'\u2265', u'\u2264', u'\u2320',
    u'\u2321', u'\u00f7', u'\u2248', u'\u00b0', u'\u2219', u'\u00b7', u'\u221a',
    u'\u207f', u'\u00b2', u'\u25a0', u'\u00a0',
};

std::string Cp437ToUtf8(const std::string& source) {
  std::u16string utf16string;
  for (size_t i = 0; i < source.length(); i++) {
    utf16string.push_back(kCp437ToUnicodeTable[static_cast<size_t>(
        static_cast<unsigned char>(source.at(i)))]);
  }
  std::string utf8result;
  base::UTF16ToUTF8(utf16string.c_str(), source.length(), &utf8result);
  return utf8result;
}
