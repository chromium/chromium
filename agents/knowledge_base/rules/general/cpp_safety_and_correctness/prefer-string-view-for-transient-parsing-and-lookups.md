---
name: general/prefer-string-view-for-transient-parsing-and-lookups
rubric: safety
severity: should_fix
---

**Rule.** Prefer string view types (`std::string_view`, `base::cstring_view`, or
subsystem view aliases) over owning `std::string` when parsing, extracting
substrings with `.substr()`, or performing map and set lookups. When querying or
inserting into associative containers configured with transparent comparators
(such as `std::less<>` or `base::flat_map`), pass the `std::string_view`
directly rather than calling `.data()` or constructing temporary `std::string`
objects.

**Why.** Calling `.substr()` on `std::string` or allocating owning `std::string`
temporaries for lookups incurs avoidable heap allocations and buffer copies.
Calling `.data()` on a `std::string_view` degrades it to `const char*`,
discarding known string bounds and forcing `strlen` recalculation or temporary
object creation in transparent containers.

## Examples

Bad:

```cpp
// Calling .substr() on std::string allocates a temporary heap string:
uint64_t p0, p1;
CHECK(base::HexStringToUInt64(uuid_str.substr(0, 8), &p0));
CHECK(base::HexStringToUInt64(uuid_str.substr(9, 4), &p1));

// Allocating owning std::string for immediate map lookup:
size_t ParseToken(const char* input, std::string* token);
std::string token;
ParseToken(env_var, &token);
auto it = config_map.find(token);

// Calling .data() on string_view discards length and forces temporary string construction:
feature_map[feature.name.data()] = &feature;
```

Good:

```cpp
// Binding to std::string_view enables non-allocating .substr() slicing:
std::string_view uuid = uuid_str;
uint64_t p0, p1;
CHECK(base::HexStringToUInt64(uuid.substr(0, 8), &p0));
CHECK(base::HexStringToUInt64(uuid.substr(9, 4), &p1));

// Using std::string_view directly for zero-allocation token extraction and map lookup:
size_t ParseToken(std::string_view input, std::string_view* token);
std::string token_str;
ParseToken(env_var, &token);
auto it = config_map.find(token);

// Passing std::string_view directly to transparent container operations:
auto [it, inserted] = feature_map.try_emplace(feature.name, &feature);
```
