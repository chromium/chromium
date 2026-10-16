---
name: general/avoid-redundant-variable-before-return
rubric: readability
severity: nit
---

**Rule.** Avoid introducing or retaining redundant single-use local variables,
references, or named constants when a value, simple expression, or constant is
only used once immediately in a return statement, call site, container
insertion, assertion, or chained accessor call. Consume, move, chain, or pass
the expression or constant directly to keep functions concise and avoid
unnecessary indirection.

**Why.** Declaring intermediate local variables solely to pass, return, move, or
chain them adds visual noise, boilerplate, and unnecessary indirection. Inlining
concise expressions, accessor chains, or trivial constants clarifies data flow,
allows rvalue temporaries to move implicitly, and keeps function bodies clean.

## Examples

Bad:

```cpp
// Redundant single-use variable before return:
uint64_t name_hash = HashMetricName(name);
return Factory(name, name_hash, flags).Build();

// Single-use intermediate variables created only to std::move:
std::string key = decode_data_func(key_values[i]);
std::string value = decode_data_func(key_values[i + 1]);
params[std::move(key)] = std::move(value);

// Trivial constant local variable in assertion or call site:
int task_start_now_samples = 6;
EXPECT_EQ(7 + GetExtraCount() + task_start_now_samples, GetTotalCount());

// Single-use intermediate reference before chained accessor:
const auto& manifest = root->GetDict();
const std::string* version = manifest.FindString("version");
```

Good:

```cpp
// Consume value-producing expression directly:
return Factory(name, HashMetricName(name), flags).Build();

// Inline value-producing expressions directly:
params[decode_data_func(key_values[i])] =
    decode_data_func(key_values[i + 1]);

// Inline constant directly into assertion or call site:
EXPECT_EQ(7 + GetExtraCount() + 6, GetTotalCount());

// Inline chained accessor directly:
const std::string* version = root->GetDict().FindString("version");
```
