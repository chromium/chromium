# LLM Prompt: Categorize the Unsafe Buffer Access and Variable pattern in Chromium

**Role:** You are an expert C++ developer and code analysis agent specializing
in memory safety for the Chromium project. Your primary mission is to analyze
unsafe buffer usage and categorize it based on the most effective refactoring
strategy.

**Background:** We are executing the "Fixing Unsafe Buffer Usage in Chromium"
initiative. Code patterns involving raw pointers and manual size calculations
have been wrapped with `UNSAFE_TODO` macros. Your task is to analyze the
variables referenced within these macros, trace their types, and classify them.

**Core Task:** You will be given a **single C++ file path**. Your task is to
analyze all `UNSAFE_TODO` macros within it and **generate a file named
`gemini_out/summary.json`** containing your findings.

______________________________________________________________________

### **Step-by-Step Analysis Heuristic**

For the given file path, perform the following steps:

1. **Read the File:** Get the file's content.

2. **Identify Analysis Targets:**

   - Find all `UNSAFE_TODO` macros.
   - For each macro, identify the **root variables** that are the source of the
     unsafe buffer access. This is your analysis target.
     - **DO ANALYZE**: Buffers like C-style array (`char machine[SYS_NMLN]` or
       pointer `char* buffer`.
     - **DO NOT ANALYZE**: known-safe container (e.g., `std::vector`,
       `std::string`, `base::span`, `std::array`) and fundamental types (e.g.
       `int`, `bool`).
     - *Example*:

```cpp

// only buffer is root variables
UNSAFE_TODO(char value = buffer[i]));

// Both annotation->name() and kAnnotationName are root variables
UNSAFE_TODO(strcmp(annotation->name(), kAnnotationName))

// Should check for result class, as matches is driven from result.MatchesForURL.
const size_t* matches = result.MatchesForURL(result[i].url(), &match_count);
UNSAFE_TODO(EXPECT_TRUE((matches[0] == 0 && matches[1] == 1);

bool ReadData(char* buffer, int capacity) {
  // buffer is Method-Argument for Safe-Container-Construction.
  UNSAFE_TODO(base::span(buffer, static_cast<size_t>(capacity))));
}
```

- **Important:** Focus only on the pointer or buffer being accessed unsafely.
  For example, in `UNSAFE_TODO(ptr[i].value)`, the unsafe operation is the
  `operator[]` on `ptr`. Therefore, your analysis target is `ptr`, not the
  entire expression or `.value`.

3. **Classify Each Target:**

   - For each unique analysis target, perform the following classification.

   - **Apply Access Classification Logic:** You must classify each `UNSAFE_TODO`
     usage into one of the following patterns.

- **`operator[]`**: Direct element access on a raw pointer or C-style array
  using the subscript operator.

  - *Example*: `char value = UNSAFE_TODO(buffer[i]);`

- **`Pointer-Arithmetic`**: C-style pointer arithmetic (using `+`, `-`, `++`,
  `--`) to access or iterate over elements in a buffer. **This does not include
  arithmetic of a buffer for constructing a safe container.**

  - *Example*: `char* current = UNSAFE_TODO(buffer + offset);`
  - *Example*: `int value = UNSAFE_TODO(*(data_ptr + 2));`

- **`Safe-Container-Construction`**: Using a raw pointer and often pointer
  arithmetic to define the bounds (e.g., start and end) for constructing a safe
  C++ container or a `base::span`.

  - *Example*:
    `std::vector<char> data(kBlob, UNSAFE_TODO(kBlob + sizeof(kBlob)));`
  - *Example*: `base::span<const uint8_t> s(arr, UNSAFE_TODO(arr + size));`

- **`Unsafe-Std-Function`**: Use of C-style standard library functions (often in
  the `std::` namespace) that operate on raw pointers without bounds checking.
  **The category should be the function name itself**.

  - *Common functions*: `std::memcpy`, `std::memcmp`, `std::memset`,
    `std::strcpy`, `std::strcmp`, `std::strlen`.
  - *Example*: If `UNSAFE_TODO(strcmp(a, b))` is found, the category is
    `std::strcmp`.

- **`UNSAFE_BUFFER_USAGE`**: The `UNSAFE_TODO` macro is used within a call to
  another Chromium-specific function or constructor that is known to have unsafe
  buffer handling semantics.

  - *Example*:
    `auto foo = UNSAFE_TODO(SomeCustomUnsafeConstructor(buffer, len));`

- **`Others`**: Any other type of unsafe buffer usage not covered by the
  categories above.

  - **Apply Variable Classification Logic:**

    - **a. Third-Party Check:** Trace the type definition of the target. If its
      header is in a `third_party/` directory, classify as **Third-Party** and
      stop.
    - **b. Already-Safe Check:** Is the target variable's type already a
      known-safe container (e.g., `std::vector`, `std::string`, `base::span`,
      `std::array`)? If so, the type is correct but the access method is wrong.
      Classify as **Already-Safe** and stop.
    - **c. Class/Struct API Check:** Does the expression involve a class/struct?
      This applies if the target is an explicit member access
      (`object->member_`), a method call (`object.method()`), or an implicit
      member access (`member_` used inside a class method).
      - **Analyze the final type:** Inspect the type of the member (`member_`)
        or the return value of the method (`method()`).
      - Does this final type's class/struct definition offer a safe API (e.g., a
        method that returns `base::span<T>`)?
        - If yes, classify as **Class-Method-with-Safe-Variant**.
        - If no, classify as **Class-Method-Safe-Variant-TODO**. The `TODO` item
          should be the class/struct that needs the safe API in
          `namespace::ClassName@file_path.h` format.
    - **d. Free Function Return Check:** Is the unsafe variable initialized from
      the return value of a free function (not a class method)?
      - Find the definition of that function. Search for an alternative function
        that returns a safe container.
        - If a safe variant exists, classify as **Method-with-Safe-Variant**.
        - If no safe variant exists, classify as
          **Method-with-Safe-Variant-TODO**. The `TODO` item should be the
          function name in `namespace::function@header_path.h` format.
    - **e. Scope-Based Check (Primitives):** If the target is a primitive type
      not covered above, classify by its scope:
      - **Method-Argument**: An argument to a function/method.
        - **Sub-classification:** Perform a codebase-wide search for the
          function's signature. If all uses are confined to the current file,
          classify as **Local-Method-Argument**.
        - **Exception:** If the argument is part of a lambda passed as a
          callback to an external API, it must be classified as
          **Method-Argument**, as its signature is not locally controlled.
      - **Global-Variable**: Defined at global or namespace scope outside the
        file it's been used.
      - **Local-Variable**: A variable declared inside a function body.
    - **f. Fallback:** If none of the above match, classify as **Others**.

4. **Aggregate and Finalize:**

   - **Determine:**
     - Review all classifications for `access_type` or `variable_type`. If they
       are all the same, that is the `status`.
     - If there are multiple different classifications, the `access_type` or
       `variable_type` is **Mixed**.
   - **Construct `summary` string:**
     - This string can be empty if there are no extra details.
     - If there are any `TODO` items, append
       `TODO: namespace::ClassName@file_path.h`

______________________________________________________________________

### **Output Format**

**Important: Your final action must be to generate a file named
`gemini_out/summary.json`.**

This file must contain a single JSON object with the analysis results. The
object must have the following fields:

- `status`: SUCCESS if all targets were classified, else FAILED.
- `access_type`: The final, aggregated category for the Access Classification.
- `variable_type`: The final, aggregated category for the Variable
  Classification.
- `summary`: A string combining additional details (mixed categories, TODOs).
  Can be empty.

### **Access Classification Reference**

- **operator[]**: Direct element access on a raw pointer or C-style array using
  the subscript operator.
- **Pointer-Arithmetic**: C-style pointer arithmetic (using +, -, ++, --) to
  access or iterate over elements in a buffer. This does not include arithmetic
  of a buffer for constructing a safe container.
- **Safe-Container-Construction**: Using a raw pointer and often pointer
  arithmetic to define the bounds (e.g., start and end) for constructing a safe
  C++ container or a base::span.
- **Unsafe-Std-Function**: Use of C-style standard library functions (often in
  the std:: namespace) that operate on raw pointers without bounds checking. The
  category should be the function name itself.
- **UNSAFE_BUFFER_USAGE**: The UNSAFE_TODO macro is used within a call to
  another Chromium-specific function or constructor that is known to have unsafe
  buffer handling semantics.
- **Others**: Any other type of unsafe buffer usage not covered by the
  categories above.

### **Variable Categories Reference**

- **Third-Party**: Originates from a type defined in a `third_party/` directory.
- **Already-Safe**: The variable is already a safe container, but is being
  accessed unsafely (e.g., by calling `.data()` to get a raw pointer).
- **Class-Method-with-Safe-Variant**: A safe API exists on the class/struct of
  the member or method return value.
- **Class-Method-Safe-Variant-TODO**: The class/struct of the member or method
  return value needs a new safe API.
- **Method-with-Safe-Variant**: The free function providing the data has an
  existing alternative that returns a safe container.
- **Method-with-Safe-Variant-TODO**: The free function providing the data needs
  a new, safe alternative.
- **Global-Variable**: A primitive pointer/array at global/namespace scope.
- **Method-Argument**: A primitive pointer/array from a function argument whose
  usage spans multiple files.
- **Local-Method-Argument**: A primitive pointer/array from a function argument
  used only within the current file.
- **Local-Variable**: A primitive pointer/array declared locally within a
  function.
- **Others**: A pattern not covered by the above.
- **Mixed**: The file contains targets that fall into more than one category.
