---
name: general/no-icu-unicodestring-for-8bit-string-manipulation
rubric: readability
severity: should_fix
---

**Rule.** Do not construct intermediate icu::UnicodeString objects solely to
convert between UTF-8 and UTF-16 or to perform basic string operations (such as
tokenization, searching, or replacement). Use Chromium's direct conversion
helpers (base::UTF16ToUTF8 and base::UTF8ToUTF16 from
base/strings/utf_string_conversions.h) and standard base/strings utilities
(e.g., base::StringTokenizer, base::ReplaceSubstringsAfterOffset, base::StrCat,
or base::SplitString).

**Why.** Transcoding strings through intermediate icu::UnicodeString objects
introduces unnecessary ICU object overhead, memory allocations, and extra buffer
copies. Operating directly with Chromium's base/strings conversion helpers and
string manipulation utilities avoids these performance costs and reduces
boilerplate.

## Examples

Bad:

```cpp
// Bad: Using intermediate icu::UnicodeString for conversions or string operations.
icu::UnicodeString ustr(text.data(), text.length());
std::string text_utf8;
ustr.toUTF8String<std::string>(text_utf8);
return UnicodeStringToString16(icu::UnicodeString::fromUTF8(res));

icu::UnicodeString id_ustr(id.data(), id.length());
icu::UnicodeString rules_ustr = id_ustr.insert(0, u"::").append(u";");
rules_ustr.toUTF8String<std::string>(rules_str);
```

Good:

```cpp
// Good: Use direct base conversion helpers and base string manipulation utilities.
std::string text_utf8 = base::UTF16ToUTF8(text);
std::u16string result = base::UTF8ToUTF16(res);

std::string rules_str;
base::StringViewTokenizer t(id, ";");
while (std::optional<std::string_view> token = t.GetNextTokenView()) {
  base::StrAppend(&rules_str, {"::", *token});
}
rules_str += ";";
```
