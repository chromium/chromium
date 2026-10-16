---
name: general/account-for-quoted-commas-when-parsing-macro-arguments
rubric: correctness
severity: should_fix
---

**Rule.** When parsing C/C++ macro invocations or function call arguments in
Python presubmit checks and source-scanning utilities, do not naively split
argument strings on commas (`.split(',')`) or match arguments using naive
comma-delimited patterns like `[^,]+`. Account for commas that appear inside
quoted string literals by using escape-aware regex patterns (e.g.,
`r'"(?:\\.|[^"\\])*"'` rather than naive `'"[^"]*"'` that fails on escaped
quotes (`\"`)), balanced parentheses for nested function calls, and balanced
angle brackets for template argument lists (e.g., `std::map<K, V>`), and
consistently reuse defined quote helpers across all argument positions.

**Why.** In C/C++, macro and function arguments frequently contain commas inside
double-quoted string literals, template argument lists, or nested function calls
(e.g., `fn(a, b)`). Naively splitting on commas or matching with `[^,]+`
fragments argument tokens, while naive quote regexes like `"[^"]*"` terminate
prematurely on escaped quotes (`\"`), corrupting argument counts and breaking
presubmit parsers.

## Examples

Bad:

```cpp
# Naive comma splitting or regex matching breaks on nested parens, templates, or strings:
for m in BASE_FEATURE_PARAM_RE.finditer(file_contents):
  args = [arg.strip() for arg in m.group(1).split(b',')]
  if len(args) == 5:
    param_name = args[3].strip(b'"')

# Naive _ARG breaks on function calls fn(a, b) or template types std::pair<T1, T2>:
_QUOTE_STR = br'"(?:\\.|[^"\\])*"'
_ARG = br'(?:[^,"]|' + _QUOTE_STR + br')*'
```

Good:

```cpp
# Helper: matches a single macro argument, handling commas inside quotes,
# balanced parens (function calls), and balanced angle brackets (templates):
_QUOTE_STR = br'"(?:\\.|[^"\\])*"'
_ARG = (br'(?:[^,()<>"]|' + _QUOTE_STR +
        br'|\((?:[^()]|' + _QUOTE_STR + br')*\)' +
        br'|<(?:[^<>]|' + _QUOTE_STR + br')*>)*')

# Reusing _QUOTE_STR and _ARG helpers consistently across signatures:
BASE_FEATURE_PARAM_5_ARGS_RE = re.compile(
    br'BASE_FEATURE_PARAM\(' + _ARG + br',' + _ARG + br',' + _ARG + br','
    br'\s*(' + _QUOTE_STR + br')\s*,(?:[^;"]|' + _QUOTE_STR + br')*?\);',
    flags=re.MULTILINE | re.DOTALL)
```
