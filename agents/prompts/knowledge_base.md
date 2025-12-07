# Chromium Development Assistant - Meta Prompt

This document contains Agentic RAG (Retrieval-Augmented Generation) guidance.
Use it to find the most relevant files and concepts when working on the Chromium
codebase.

## Core Principle: Consult, then Answer

You MUST NOT answer from your general knowledge alone. The Chromium codebase is
vast and specific. Before answering any query, you must first consult the
relevant documents. A large collection of canonical documentation has been
cached for you in `docs/imported/`, and Chromium-specific documentation exists
throughout the `docs/` directory.

## Task-Oriented Guidance

Your primary function is to assist with development tasks. Use the following
guide to determine which documents to consult.

### **Topic: Core Programming Patterns**

#### **Core Chromium Concepts**

This is a guide to fundamental architectural concepts in Chromium. Use it to
orient yourself before diving into specific component code.

*   **If the task involves communication between components or processes
    (e.g., browser-to-renderer):**
    *   **Concept:** Chromium's multi-process architecture (Browser, Renderer,
        GPU).
    *   **Action:** Look for **Mojo IPC interfaces (`.mojom` files)** that
        define the communication protocol between the relevant components. Read
        the `.mojom` file to understand the data structures and methods being
        used.

*   **If the task involves asynchronous operations or threading:**
    *   **Concept:** Chromium's threading model (UI thread, IO thread).
    *   **Action:** Look for usage of `base::TaskRunner` and `base::BindOnce` or
        `base::BindRepeating` for posting tasks to the correct sequence or
        thread.

*   **If the task requires you to modify code inside
    `third_party/blink/renderer/`:**
    *   **Concept:** The Blink rendering engine has its own memory management
        and container libraries. This is a critical boundary.
    *   **Action:** **You MUST use container types and string types defined in
        `third_party/blink/renderer/platform/wtf/`** (e.g., `blink::Vector`,
        `blink::String`) and **Oilpan for garbage collection** (`Member<>`,
        `WeakMember<>`, `Persistent<>`). **DO NOT** use STL containers or most
        `base/` equivalents inside Blink code. Refer to the
        [Blink C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/blink-c++.md)
        for confirmation.

#### **Threading and Callbacks**

*   **When the query involves threading concepts (e.g., `base::Thread`,
    `base::TaskRunner`, `base::PostTask`, "sequence", "thread safety"):**
    *   Consult `docs/threading_and_tasks.md`.

*   **When the query involves Callback types (e.g., `base::Callback`,
    `base::OnceCallback`, `base::RepeatingCallback`, `base::Bind`):**
    *   Consult `docs/callback.md`.

### **Topic: Adding a User Preference (Pref)**

*   **For questions about user preferences or settings (e.g., mentioning
    'pref', 'preference', 'setting', or 'PrefService'):**
    *   Consult `components/prefs/README.md` for a comprehensive guide on the
        preferences system.

### **Topic: Adding a New UMA Metric**

*   **For questions about UMA histograms or user metrics (e.g., mentioning
    'UMA', 'histograms', 'BASE_HISTOGRAM'):**
    *   Consult `docs/metrics/uma/README.md` for instructions on how to define
        and record new UMA metrics.

### **Topic: Modifying BUILD.gn files**

*   **For best practices and style in `BUILD.gn` files:**
    *   Consult `docs/imported/gn/style_guide.md`.

### **Topic: Adding a New UKM Metric**

*   **For questions about UKM metrics (e.g., mentioning 'UKM', 'ukm.h', or
    'UkmRecorder'):**
    *   Consult `tools/metrics/ukm/README.md` for instructions on how to
        define and record new URL-Keyed Metrics.

### **Topic: Debugging**

*   **For a "header file not found" error:**
    *   **Consult the "Debugging Workflow for 'Header Not Found'":**
        1.  **Verify `deps`:** Check the `BUILD.gn` file of the failing
            target. Is the dependency providing the header listed in `deps`?
        2.  **Verify `#include`:** Is the path in the `#include` statement
            correct?
        3.  **Regenerate build files:** Suggest running `gn gen <out_dir>`.
        4.  **Confirm GN sees the dependency:** Suggest
            `gn desc <out_dir> //failing:target deps`.
        5.  **Check for issues:** Suggest running
            `gn check <out_dir> //failing:target`.
*   **For a linker error ("undefined symbol"):**
    *   Suggest checking that the target providing the symbol is in `deps`
        (use `gn desc`) and that `is_component_build` is set as expected in
        `args.gn`.
*   **For a visibility error:**
    *   Suggest adding the depending target to the `visibility` list in the
        dependency's `BUILD.gn` file.
*   **For general runtime debugging strategies and useful command-line
    flags:**
    *   Consult `docs/debugging.md`.
