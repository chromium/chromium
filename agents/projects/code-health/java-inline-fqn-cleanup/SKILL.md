---
name: java-inline-fqn-cleanup
description: Identify and clean up inline fully qualified Java names (FQNs) in first-party code, replacing them with standard imports.
---

# Code Health: Java Inline FQN Cleanup

Clean up inline fully qualified names (FQNs) in Java code by replacing them with
proper import statements at the top of the file, and then formatting/optimizing
the import order.

## Overview

Using fully qualified class names inline (e.g. `android.view.View view = ...`)
instead of importing them makes code harder to read and breaks standard style
conventions. This skill helps automate the discovery, safety analysis, and
clean-up of these inline qualifiers in Chromium's first-party Java files.

## 📂 Resources

- **Discovery Script:** [find_candidates.py](scripts/find_candidates.py)
- **Automated Review Protocol:**
  [automated_review.md](references/automated_review.md)
- **Shared Workflows:**
  [shared_workflows.md](../hub/references/shared_workflows.md) (Validation,
  Uploading, CL Creation)

## 📋 Examples & Patterns

### 1. Variables & Types

- **Before:** `android.view.View view = ...`
- **After:** `View view = ...` (with `import android.view.View;` added at top)

### 2. Static Methods & Constants

- **Before:** `org.chromium.build.BuildConfig.IS_CHROME_BRANDED`
- **After:** `BuildConfig.IS_CHROME_BRANDED` (with
  `import org.chromium.build.BuildConfig;` added at top)

### 3. Annotation Markers

- **Before:**
  `@OptIn(markerClass = org.chromium.net.QuicOptions.Experimental.class)`
- **After:** `@OptIn(markerClass = QuicOptions.Experimental.class)` (with
  `import org.chromium.net.QuicOptions;` added at top)

### 4. Method Parameters

- **Before:** `void foo(org.chromium.base.Callback<T> callback)`
- **After:** `void foo(Callback<T> callback)` (with
  `import org.chromium.base.Callback;` added at top)

### 5. Implicitly Imported Classes (java.lang.\*)

- **Before:** `java.lang.String text = ...`
- **After:** `String text = ...` (**DO NOT** add an import statement since
  `java.lang.*` is implicitly imported by Java. Adding it will cause a
  RedundantImport linter error.)

### 6. Resource Classes (*.R.*)

- **Rule:** **DO NOT** modify any FQN containing `.R.` (e.g., `android.R.attr`
  or `org.chromium.chrome.R.id`).
- **Why:** Local `R` classes are almost always imported (e.g.,
  `import org.chromium.chrome.R;`). Importing a secondary `R` class (like
  `android.R`) creates an unresolvable naming collision that breaks the build.
  These MUST remain fully qualified.

## Workflow

### Workspace Preparation

- **Clean & Update:** Follow the **Workspace Preparation** section in
  `../hub/references/shared_workflows.md` to ensure a clean and updated
  environment.

### Discovery & Batch Selection

1. **Discovery:** Run the candidate discovery script from the skill's `scripts/`
   folder:

   ```bash
   python3 scripts/find_candidates.py
   ```

2. **Present Candidate Batch:** You MUST output the candidate batch details to
   the user and request confirmation to proceed. Announce the batch with exactly
   this format (replace with details):

   - **Batch Directory:** [Directory]
   - **File Count:** [File Count]
   - **Files:**
     - [File 1]
     - [File 2]
     - ...
   - **Imports Found:**
     - [Import 1]
     - [Import 2]
     - ...
   - **Banned FQNs:** (If applicable)
     - [Banned 1]
     - [Banned 2]
     - ...
   - **Proceed Confirmation**: Prompt the user using standard text asking if you
     should proceed with cleaning up this batch.

### Branch Creation

- Follow the **Branch Creation** section in
  `../hub/references/shared_workflows.md` using the branch name
  `cleanup-inline-imports-[component-name]` (e.g.
  `cleanup-inline-imports-tab-ui`).

### Refactoring & Implementation

**CRITICAL RULE:** You MUST use your standard file editing tools (like
`replace_file_content` or `multi_replace_file_content`) to apply the cleanups
directly to the Java files. **DO NOT** create, write, or execute any custom
python or bash scripts to perform the text replacements.

For each file in the batch, apply the following cleanup rules:

1. **Avoid Name Collisions & Shadowing:** Check if the simple class name of the
   FQN is already imported, defined, or implicitly resolved in that file (e.g.,
   via wildcard imports, implicit package-private classes, or other imported
   annotations with the same name). If there is a collision or shadowing risk,
   **do not modify the line** (the FQN must remain inline).
2. **Banned FQNs:** If the discovery script outputted a `Banned FQNs` list, you
   must completely ignore those specific FQNs. They are either raw package paths
   or invalid classes, and attempting to import them will break the build.
3. **First-Party Code Only:** Ensure you do not modify any files in
   `third_party/` or auto-generated directories.
4. **Import Strategy for Static Members:** When cleaning up inline FQNs for
   static constants, properties (e.g. `TabProperties`), feature flags
   (`ChromeFeatureList`), or enums (`TabLaunchType`), **prefer importing the
   class itself rather than importing its members statically**.
   - *Incorrect (Static Import):*
     `import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.IS_SELECTED;`
   - *Correct (Class Import):*
     `import org.chromium.chrome.browser.tasks.tab_management.TabProperties;`
     and use `TabProperties.IS_SELECTED` in the code.
   - *Rationale:* Qualifying static constants (e.g. `TabProperties.TITLE`) keeps
     clear context of where the constant is defined and avoids namespace
     conflicts when multiple imported properties classes share the same field
     names (e.g. `TabProperties.TITLE` vs `FolderProperties.TITLE`).
5. **Clean up Javadocs & Comments:** Replace inline FQNs found inside code
   comments and Javadoc links (e.g. `{@link android.webkit.WebSettings}` ->
   `{@link WebSettings}`). However, **never modify URLs** (like `https://...` or
   other web links) inside comments.
6. **Nested and Inner Classes:** For inline references to nested or inner
   classes (e.g. `org.chromium.chrome.browser.profiles.ProfileKey.Theme`),
   prefer importing the top-level outer class
   (`import org.chromium.chrome.browser.profiles.ProfileKey;`) and referring to
   it as `ProfileKey.Theme` in the code, rather than importing the nested class
   directly.
   - *Exception:* If importing the nested class directly is the dominant style
     in that specific file, match the existing style.
7. **Apply Replacements:**
   - Replace the inline FQN with its simple class name.
   - Add the corresponding `import` statement at the top of the file.

### Verification & Validation

1. **Code Formatting:** Execute `git cl format` to format the modified source
   code and organize imports. Address any errors that are reported.
2. **Build & Test Verification:**
   - Proactively request the user to compile and run tests on the affected
     module/targets to ensure no regressions or ambiguities.
3. **Mandatory Final Review:** Execute the **Shared Automated Review Protocol**
   defined in `../hub/references/shared_automated_review.md`, using the specific
   prompt overrides in `references/automated_review.md`. Do NOT skip this step.
   Do NOT proceed to the Submission phase until the review returns `PASS`.

### Submission

1. **Bug Tracking:**

   - File a new Buganizer issue using the `create_buganizer_issue` tool with the
     following properties:
     - **Title:** Clean up inline fully qualified Java names in
       [Component/Directory]
     - **commentMarkdown:** Automated cleanup of inline fully qualified Java
       class names (replacing them with standard imports) in the
       `[Component/Directory]` directory.
     - **componentId:** `1456931` (Chromium > CodeHealth)
     - **hotlistIds:** `["8218789"]`
     - **issueType:** `INTERNAL_CLEANUP`
     - **priority:** `P2`
     - **assignee:** Set to the user's LDAP email (e.g. `username@google.com`)
   - Retrieve the new Bug ID from the created issue and use it in the commit
     message.

2. **Commit**: Follow the **Commit** section in
   `../hub/references/shared_workflows.md` using the following commit message
   template:

   ```
   [code-health] Clean up inline FQNs in [Component/Directory]

   Remove inline fully qualified class names and replace them with
   standard imports.

   This change was generated using the skill java-inline-fqn-cleanup.

   Bug: 528570333, <BugID>
   ```

3. **Submission Pipeline:** Follow the **Upload to Gerrit** section in
   `../hub/references/shared_workflows.md` to handle the upload.

4. **Workspace Reset:** Switch back to `main`: `git checkout main`.

5. **Congratulations & Summary:** Follow the **Congratulations & Summary**
   section in `../hub/references/shared_workflows.md`. For this skill, the
   **[Specific Cleanup Details]** are:

   - **Cleaned Component/Directory:** The parent directory of the batch.
   - **File Count:** Number of files cleaned up.
