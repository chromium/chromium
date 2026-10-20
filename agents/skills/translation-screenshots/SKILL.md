---
name: translation-screenshots
description: >-
  Identifies new or updated translatable strings in a Chromium change (.grd/.grdp),
  guides capturing appropriate UI screenshots with sufficient surrounding context,
  and uploads them to Google Cloud Storage via tools/translation/upload_screenshots.py
  to generate and stage .png.sha1 hash files.
---

# Chromium Translation Screenshots

Follow this skill when adding or updating user-facing translatable strings in
Chromium (`.grd` or `.grdp` files). Translation screenshots provide essential
visual and semantic context to human translators during localization.

> [!IMPORTANT] **Execute Without Pausing for Manual Review**: This skill must be
> executed autonomously end-to-end without pausing for manual review or treating
> screenshot capture/upload as a developer handoff task, unless genuinely
> blocked by an unrecoverable technical or permission issue. The agent should
> generate or capture the screenshots (using test fixtures, reference assets, or
> the `image-generator` subagent), upload them using `upload_screenshots.py`,
> stage the resulting `.sha1` hashes, clean up temporary images, and proceed
> directly to presubmit verification.

______________________________________________________________________

## 1. Identify New or Modified Strings

1. **Inspect changes in resource files**: Find all modified or added `.grd` and
   `.grdp` files in the working branch:

   ```bash
   git diff origin/main...HEAD --name-only -- "*.grd" "*.grdp"
   ```

   Or for uncommitted changes:

   ```bash
   git diff --name-only -- "*.grd" "*.grdp"
   ```

2. **Check string definitions**: For each changed
   `<message name="IDS_..." ...>`:

   - **Translatable strings**: If `translateable="false"` is **not** present,
     the string is translatable and **requires** a translation screenshot.
   - **Untranslatable strings**: If `translateable="false"` is set (e.g.,
     internal debug strings, brand names, or temporary prototypes), **do not**
     create screenshot hashes.
   - **Existing strings**: If only C++ / WebUI code changed but the string
     definition in `.grd` was not modified, no new screenshot is needed. If the
     string copy was modified, an updated screenshot is required.
   - **Conditionally defined messages (e.g., titlecase or OS)**: If a message is
     defined conditionally across `<if expr="...">` blocks (such as platform
     variants for macOS, Windows, Linux, or `use_titlecase`), it is completely
     acceptable to upload a screenshot for just **one** case or platform.
     Because the screenshot file is keyed directly to the message name
     (`<MESSAGE_NAME>.png.sha1`), a single screenshot satisfies the requirement
     for all conditional branches sharing that ID.
   - **Cross-platform sharing**: When sharing an existing desktop string with
     Android/iOS, retain the existing platform screenshot; do not overwrite
     desktop screenshots with mobile ones unless the UI definition changed.

3. **Search for existing strings & messages with approved tools**: Always use
   approved built-in tool APIs (`code_search`, `read_file`) and sandboxed
   commands (`git grep`) to search and inspect `.grd`/`.grdp` files. Never run
   custom Python scripts (`python3 -c ...`) or ad-hoc shell commands that
   require manual user approval to inspect or search files.

   - **Search for exact message text** across resource files:
     ```bash
     git grep -n -E "^\s*<Exact Text>\s*$" -- "*.grd" "*.grdp"
     ```
   - **Search for message IDs or partial names**:
     ```bash
     git grep -n "IDS_<MESSAGE_NAME>" -- "*.grd" "*.grdp"
     ```
   - **Code Search**: Use the `code_search` tool with queries like
     `f:\.grd "<Exact Text>"` or `f:\.grd "IDS_<MESSAGE_NAME>"`.
   - **Read File**: Use `read_file` to inspect the full XML message block,
     including `<message>` tags, attributes, and surrounding
     comments/descriptions.

4. **Accessibility Labels & Non-Rendered UI Strings**: Some strings are used as
   accessibility labels and are never visually rendered on-screen, but are
   conveyed to users through text-to-speech (TTS), screen readers, braille
   displays, or other assistive technology.

   - **Accessibility attributes still require screenshots**: Attributes such as
     `aria-label`, `title`, or accessibility descriptions are attached to
     controls that users still encounter in the interface. These **must** still
     have a translation screenshot showing the target control and its
     surrounding context.

   - **Provide locator hints in the message description**: When a string is
     conveyed through assistive technology and not visually rendered, provide a
     clear hint in the `<message>` `desc="..."` attribute to help the translator
     locate the control within the screenshot.

     *Example description*:

     ```xml
     <message name="IDS_SETTINGS_CLEAR_DATA_BUTTON_A11Y_LABEL"
         desc="Spoken by screen readers when the clear browsing data button gets focus but not visually rendered.">
       Clear browsing data
     </message>
     ```

   - **Exempting strings with NO associated UI
     (`is_accessibility_with_no_ui="true"`)**: If you must include a message
     string without a screenshot, it should **not** be associated with any UI.
     In the rare case of a string truly lacking any associated UI, add
     `is_accessibility_with_no_ui="true"` to the `<message>` element:

     ```xml
     <message name="IDS_EXAMPLE_ARIA_STRING"
         is_accessibility_with_no_ui="true"
         desc="Notification spoken when background sync completes with no visual notification or UI element.">
       Sync complete
     </message>
     ```

     This informs Chromium's presubmit check that the string intentionally has
     no corresponding screenshot hash.

5. **Determine target screenshot directory and filename**: Screenshots are
   stored in a companion directory named after the resource file (replacing dots
   with underscores):

   - `chrome/app/generated_resources.grd` $\\rightarrow$
     `chrome/app/generated_resources_grd/`
   - `chrome/app/settings_strings.grdp` $\\rightarrow$
     `chrome/app/settings_strings_grdp/`
   - Target filename: `path/to/<resource_file>_<ext>/<MESSAGE_NAME>.png`
   - Example:
     `chrome/app/generated_resources_grd/IDS_ORGANIZER_PANEL_OPTIONS.png`

______________________________________________________________________

## 2. Capture the UI Screenshot

### Framing & Quality Requirements

- **Format**: `.png` only (lowercase extension).
- **Accurate Copy**: The text displayed in the screenshot must match the
  `.grd`/`.grdp` string definition exactly.
- **Sufficient Surrounding Context**: Do not tightly crop to just the button or
  label. Include surrounding UI elements, dialog frames, or adjacent panel
  sections so translators can understand the layout hierarchy and user flow.
- **Tooltips & Hover States**: If the string is a tooltip (`title` or hover
  callout), capture the element in its hovered state with the tooltip callout
  box clearly visible adjacent to the control.
- **Accessibility & Non-Rendered Labels**: If the string is an accessibility
  label (e.g. `aria-label`), capture the corresponding UI control in its normal
  or focused state in the window, providing the context described in the message
  description.
- **Conditional Variants**: For strings with conditional variants (e.g., OS
  differences or sentence-case vs title-case), taking a screenshot on your
  current development platform or for one representative case is sufficient.
- **Menus & Dropdowns**: If the string is a menu item, capture the menu opened
  against its anchor button and parent window.

### Capture Methods

#### Option A: Component Test Case (Preferred for Isolated WebUI / Views)

Component tests often render the element in the exact state needed:

- **WebUI Components**:
  - WebUI mocha tests (e.g.,
    `chrome/test/data/webui/<feature>/<component>_test.ts`) instantiate elements
    with mock data.
  - Test suites can be rendered via browser tests
    (`out/Default/browser_tests --gtest_filter="<Suite>.*"`) or loaded in Chrome
    via `chrome://<host>/test_loader.html?module=<test_path>`.
  - Screenshots can also be generated or composited using standard test
    fixtures, the `image-generator` subagent, or existing image assets without
    requiring manual permission prompts for ad-hoc Python scripts.
- **Views / Pixel Tests**:
  - Interactive UI tests (`interactive_ui_tests`) or Views unit tests
    (`views_unittests`) can render dialogs or bubbles with test fixtures.

#### Option B: Live Browser Instance

- Launch a local build:
  ```bash
  out/Default/chrome
  ```
- Navigate to or trigger the UI containing the string.
- Capture a screenshot of the region containing the element and surrounding
  context, and save it as `path/to/<file>_<ext>/<MESSAGE_NAME>.png`.

______________________________________________________________________

## 3. Upload to Google Cloud Storage

1. **Verify Detection (Dry Run)**: Ensure `upload_screenshots.py` discovers the
   newly created `.png` file:

   ```bash
   python3 tools/translation/upload_screenshots.py --dry-run
   ```

2. **Upload the Screenshot**: Execute the upload script:

   ```bash
   python3 tools/translation/upload_screenshots.py
   ```

   > [!IMPORTANT]
   >
   > - **Network Access Required**: Uploading to
   >   `gs://chromium-translation-screenshots` requires outbound network access.
   >   In agent environments running under terminal sandboxes, run this command
   >   with `BypassSandbox: true` (or run it outside the sandbox) so `gsutil`
   >   does not hang.
   > - **Account Requirement**: Only `@google.com` accounts have write
   >   permissions to the bucket.
   > - **Interactive Prompts**: The script asks confirmation to upload (`y/N`)
   >   and whether to add the `.sha1` to the CL (`Y/n`). These can be piped if
   >   automating:
   >   ```bash
   >   printf "yes\nyes\n" | python3 tools/translation/upload_screenshots.py
   >   ```

3. **Generated Hash**: The script generates `<MESSAGE_NAME>.png.sha1` containing
   the 40-character SHA-1 checksum in the screenshot directory.

4. **Display Screenshot Link to User (Google Employees)**: After generating and
   uploading the screenshots, display the link(s) to the user if they are a
   Google employee:

   - **How can I view uploaded screenshots?** If you are a Google employee, use
     the URL `http://go/translation-hash/<sha1>` (replace `<sha1>` with the
     40-character SHA-1 checksum from the `.sha1` file).

______________________________________________________________________

## 4. Git Hygiene and Staging

1. **Stage the `.sha1` file**:

   ```bash
   git add path/to/<resource_file>_<ext>/<MESSAGE_NAME>.png.sha1
   ```

2. **Remove the Raw `.png`**: Never commit the raw `.png` image file to the
   Chromium repository. Remove or move the image file after upload:

   ```bash
   rm path/to/<resource_file>_<ext>/<MESSAGE_NAME>.png
   ```

3. **Verify Git Status**: Confirm that only the `.png.sha1` file is staged, and
   no `.png` files remain untracked:

   ```bash
   git status
   ```

4. **Verify Hash Validity**: Ensure the `.sha1` file contains a valid
   40-character hex hash and is not empty or filled with placeholder zeros:

   ```bash
   cat path/to/<resource_file>_<ext>/<MESSAGE_NAME>.png.sha1
   ```

5. **Provide Viewing Link in Response**: When reporting completed screenshot
   uploads to the user, include the viewing URL for each screenshot if the user
   is a Google employee: `http://go/translation-hash/<sha1>`
