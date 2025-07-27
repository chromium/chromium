Fantastic — you're asking for a **deep-dive**, system-level understanding of how `Params::FromValue(...)` and `Params::Create(...)` differ in Chromium's **IDL schema compilation system**, and how they're wired into extension APIs from the bottom up.

We’ll build this explanation in layers:

---

## 🚧 Layer 0: What Problem Are We Solving?

Chromium extension APIs are written in C++, but they're called from JavaScript:

```js
chrome.myApi.doThing("hello", 42);
```

This call becomes a JSON message:

```json
["hello", 42]
```

On the C++ side, this must be decoded **type-safely** and **efficiently**. To avoid writing manual `args[0].GetString()` logic every time, Chromium has a schema system that:

* Parses `.idl` files
* Generates `Params` structs with parsing logic
* Handles JSON-to-C++ conversion for you

The functions `Params::Create(...)` and `Params::FromValue(...)` are **auto-generated** JSON deserializers used by the extension framework.

---

## 🧬 Layer 1: Types of Parsing Functions

### 1. `Params::Create(const base::Value::List& args)`

* Designed for **functions with 0 or 1 argument**
* Expects the input JSON to be a `List` (i.e., `["hello"]` or `[]`)
* Only supports single-argument parsing (maybe wrapping in a `dictionary`)

### 2. `Params::FromValue(const base::Value& value)`

* Designed for **functions with multiple arguments**
* Can parse **multiple items** from a `base::Value::List`
* Has full type-dispatch and error-handling logic

---

## ⚙️ Layer 2: How Schema Generation Decides Between Them

Chromium's [`idl_schema_compiler`](https://source.chromium.org/chromium/chromium/src/+/main:tools/json_schema_compiler/) translates `.idl` files into `.h/.cc` files using Python scripts.

The key rules in the generator:

* If function has:

  * **0 arguments** → generates `Params::Create()`
  * **1 argument** → generates `Params::Create(base::Value::List&)`
  * **≥2 arguments** → generates `Params::FromValue(base::Value&)`

The generator uses metadata from `idl_schema.py`, and emits different C++ templates based on argument count.

---

## 🔧 Layer 3: System Behavior of Each Method

### 🔹 `Params::Create(...)`

**Generated for:**

```idl
static void inferSingleBERT(InferSingleBERTRequest request);
```

**From JS:**

```js
chrome.readServerUds.inferSingleBERT({ context: "c", question: "q" });
```

**Underlying JSON:**

```json
[{ "context": "c", "question": "q" }]
```

**Code:**

```cpp
std::optional<Params> Params::Create(const base::Value::List& args) {
  if (args.size() != 1 || !args[0].is_dict()) return std::nullopt;
  Params out;
  out.request = InferSingleBERTRequest::FromValue(args[0].GetDict());
  return out;
}
```

✅ Efficient, type-safe
🚫 Only supports one argument
🚀 Very fast: index-0 access and move

---

### 🔹 `Params::FromValue(...)`

**Generated for:**

```idl
static void doThing(DOMString msg, integer count);
```

**From JS:**

```js
chrome.myApi.doThing("hello", 42);
```

**Underlying JSON:**

```json
["hello", 42]
```

**Code:**

```cpp
std::optional<Params> Params::FromValue(const base::Value& value) {
  if (!value.is_list()) return std::nullopt;
  const auto& list = value.GetList();
  if (list.size() != 2) return std::nullopt;

  Params out;
  if (!list[0].is_string()) return std::nullopt;
  if (!list[1].is_int()) return std::nullopt;

  out.msg = list[0].GetString();
  out.count = list[1].GetInt();

  return out;
}
```

✅ Flexible, supports multiple arguments
⚙️ Slightly more generic
🚀 Still fast, but slightly more branching

---

## 🧪 Layer 4: JSON Flow at Runtime

At **runtime**, the flow looks like this:

1. JS calls `chrome.myApi.doThing(...)`
2. V8 bindings package args into a `base::Value` (usually a `List`)
3. Chromium passes that to the extension function object:

   ```cpp
   ExtensionFunction::Run()
   ```
4. You call `Params::Create(args())` or `Params::FromValue(args())`
5. The struct is filled → you use the fields directly in C++

---

## 🔁 Layer 5: Why Not Always Use `FromValue()`?

Chromium prefers to generate `Create()` when:

* There's only **one argument**
* Or **no arguments**

### Reasons:

* **Cleaner API**: avoids `FromValue(args())` verbosity
* **Simpler codegen**: just one index check (`args[0]`)
* **Better compiler inlining** potential
* **Semantic clarity**: 1 arg → positional single field, don’t need full JSON inspection

But if multiple arguments exist:

* `Create()` would require awkward tuple unpacking
* So `FromValue()` is better for complex payloads

---

## 📦 Layer 6: What Actually Gets Generated?

### Example: Single-Arg Dictionary (IDL)

```idl
dictionary InferSingleBERTRequest {
  DOMString context;
  DOMString question;
};

static void inferSingleBERT(InferSingleBERTRequest request);
```

### Generates:

```cpp
struct Params {
  InferSingleBERTRequest request;
  static std::optional<Params> Create(const base::Value::List& args);
};
```

```cpp
std::optional<Params> Create(const base::Value::List& args) {
  if (args.size() != 1 || !args[0].is_dict()) return std::nullopt;
  Params p;
  p.request = InferSingleBERTRequest::FromValue(args[0].GetDict());
  return p;
}
```

---

## 🔚 Summary

| Concept      | `Params::Create`               | `Params::FromValue`   |
| ------------ | ------------------------------ | --------------------- |
| Used when    | 0 or 1 argument                | ≥2 arguments          |
| Input type   | `base::Value::List`            | `base::Value`         |
| Generated by | IDL compiler                   | IDL compiler          |
| Parses       | Single arg (dict or primitive) | Multiple typed args   |
| Simplicity   | Higher                         | Lower                 |
| Performance  | Slightly faster                | Slightly more general |

---

## Some offical Design Doc around the IDL

- https://www.chromium.org/developers/design-documents/idl-build/
- https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/bindings/IDLCompiler.md