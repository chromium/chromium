---
name: general/avoid-string-conversion-before-hashing
rubric: safety
severity: should_fix
---

**Rule.** When hashing objects, containers, or strings that have an underlying
binary representation (such as base::Uuid, tokens, std::wstring, or
std::u16string), hash the raw byte span directly (e.g., using
base::as_byte_span(...) or .GetBytes()) with functions like base::FastHash or
base::PersistentHash. Do not format or convert the object into an intermediate
std::string (e.g., via uuid.AsLowercaseString(), base::WideToUTF8(), or
base::UTF16ToUTF8()) solely to pass it to a hash function.

**Why.** Formatting objects or transcoding wide/UTF-16 strings into intermediate
std::strings incurs heap allocations, string formatting loops, and character
transcoding overhead. Passing raw byte spans directly avoids temporary
allocations and hashes fewer bytes (e.g., 16 raw UUID bytes versus 36 formatted
characters), significantly improving hashing performance.

## Examples

Bad:

```cpp
// Bad: Formatting or transcoding to intermediate strings before hashing.
size_t UuidHash::operator()(const base::Uuid& uuid) const {
  return base::FastHash(uuid.AsLowercaseString());
}

std::string machine_guid_utf8;
if (base::WideToUTF8(machine_guid.c_str(), machine_guid.length(),
                     &machine_guid_utf8)) {
  return base::PersistentHash(machine_guid_utf8) % 2 == 0;
}
```

Good:

```cpp
// Good: Direct hashing of raw byte spans without intermediate string conversions.
size_t UuidHash::operator()(const base::Uuid& uuid) const {
  if (!uuid.is_valid()) {
    return 0;
  }
  return base::FastHash(uuid.GetBytes());
}

return base::PersistentHash(base::as_byte_span(machine_guid)) % 2 == 0;
```
