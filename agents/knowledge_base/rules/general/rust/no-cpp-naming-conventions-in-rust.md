---
name: general/no-cpp-naming-conventions-in-rust
rubric: readability
severity: should_fix
---

**Rule.** In Rust code, constants, macros, and API definitions, follow standard
Rust naming conventions: use lowercase `snake_case!` for declarative
(`macro_rules!`) and procedural macros, and `SCREAMING_SNAKE_CASE` for constants
and static variables. Do not carry over C++ naming conventions—such as
`ALL_CAPS!` for macros, `k`-prefixed camelCase (`kConstantName`) for constants,
or enforcing a 'k' prefix in Rust feature identifier validation logic.

**Why.** Chromium and public Rust style guidelines require idiomatic Rust naming
(`snake_case` for macros and functions, `SCREAMING_SNAKE_CASE` for constants).
Porting C++ preprocessor `ALL_CAPS` conventions or `kPrefix` constant
conventions into Rust creates unidiomatic Rust code and friction for developers.

## Examples

Bad:

```cpp
// Bad: Using C++ ALL_CAPS preprocessor convention for Rust macros
#[macro_export]
macro_rules! BASE_FEATURE {
    ($id:ident, $default:expr) => { ... };
}

// Bad: Using C++ kCamelCase convention for Rust constants
pub const kMaxRetries: usize = 5;

// Bad: Enforcing C++ 'k' prefix naming convention in Rust feature validation
let bytes = name.as_bytes();
if bytes[0] != b'k' {
    panic!("Feature identifier must start with 'k'");
}
```

Good:

```cpp
// Good: Follow standard Rust snake_case for macros
#[macro_export]
macro_rules! base_feature {
    ($id:ident, $default:expr) => { ... };
}

// Good: Follow standard Rust SCREAMING_SNAKE_CASE for constants
pub const MAX_RETRIES: usize = 5;

// Good: Accept standard Rust identifiers without requiring C++ 'k' prefix
pub const unsafe fn from_id(
    name: &'static str,
    default_state: FeatureState,
    _handshake: internal::FeatureMacroHandshake,
) -> Self { ... }
```
