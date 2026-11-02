---
name: add-vector-icon
description: >-
  Automates adding and updating Chromium .icon vector icon files from Google
  Symbols (GM3). Trigger this skill whenever asked to add, create, import, or
  update a .icon file, register a vector icon in BUILD.gn
  (aggregate_vector_icons), or convert a Material Symbol / GM3 icon into a
  Chromium kFooIcon vector icon.
---

# Adding Chromium `.icon` Vector Icons

Automate fetching, converting, combining sizes, and registering `.icon` files in
Chromium using the bundled self-contained converter script
([fetch_icon.py](scripts/fetch_icon.py)).

All GM3 / Google Symbols icons in Chrome Desktop Views **must** use **100 ROND
(`rond100`, Rounded)** and **400 weight** at the required optical/canvas size
(`16px`, `20px`, or `24px`), in either **Outlined** or **Filled** (`--fill`)
style (`go/chrome-gm3-icons`).

______________________________________________________________________

## Step 1: Analyze Call Sites and Existing Icons

Before generating a new `.icon` file, inspect the CL / C++ call sites and check
for pre-existing icons:

1. **Determine All Required Icon Sizes (`16`, `20`, `24`):**

   - Search the CL / target files for references to the icon constant (e.g.
     `kFooBarIcon`) in calls such as:
     - `ui::ImageModel::FromVectorIcon(kFooBarIcon, color_id, icon_size)`
     - `gfx::CreateVectorIcon(kFooBarIcon, size, color)`
     - `views::ImageView` / `VectorIconModel` / menu item icon helpers
   - **Single size:** If the icon is used at a single DIP size (e.g. `20`),
     include only that size (`--sizes 20`). If no explicit size argument is
     passed at the call site, check the default icon size of that UI surface or
     neighboring `.icon` files (typically `20` or `16` in desktop Chrome UI).
   - **Multiple sizes:** If the icon is used at multiple sizes across the CL
     (for example, `16` in a menu item and `20` in a toolbar button), pass **all
     relevant sizes** (`--sizes 20 16`). The script outputs them in strictly
     descending order (`CANVAS_DIMENSIONS, 20,` followed by
     `CANVAS_DIMENSIONS, 16,`), as required by
     `//components/vector_icons/aggregate_vector_icons.py`.

2. **Determine Style (`Outlined` vs. `Filled`) & Filename:**

   - The `.icon` filename maps directly to the generated C++ identifier:
     `foo_bar.icon` -> `kFooBarIcon`.
   - Use the Google Symbols icon identifier as the base filename (e.g.
     `account_circle` -> `account_circle.icon`).
   - For filled variants (`--fill`), Chromium convention appends `_filled` to
     the filename: `account_circle_filled.icon` -> `kAccountCircleFilledIcon`.

3. **Check for a Pre-existing Icon with the Same Name and Size (Reuse First):**

   - Before creating a new `.icon` file, search the repository for an existing
     `<icon_name>.icon` (or `<icon_name>_filled.icon`), especially in
     `//components/vector_icons/`, `//ui/views/vector_icons/`,
     `//chrome/app/vector_icons/`, and `//ash/resources/vector_icons/`.
   - **Same name + required size already exists:** Reuse the existing icon
     constant (e.g. `vector_icons::kFooBarIcon` from
     `//components/vector_icons/`) rather than adding a duplicate `.icon` file.
   - **Same name exists in target directory, but missing required size:** Fetch
     only the missing size block with `fetch_icon.py` and splice it into the
     existing `.icon` file in descending `CANVAS_DIMENSIONS` order so you
     **preserve the original copyright year, any `FLIPS_IN_RTL` directives, and
     all existing `CANVAS_DIMENSIONS` blocks**.

4. **Choose the Target `vector_icons` Directory (Default to
   `//components/vector_icons/`):**

   - **Default (`//components/vector_icons/`):** Skew toward placing new icons
     in `//components/vector_icons/` by default. This is the most common target
     directory and provides maximum reusability across Chromium components and
     platforms.
   - **Subsystem-specific exceptions:** Only place an icon in a narrower
     directory when it is specifically tied to that layer or subsystem:
     - `//chrome/app/vector_icons/` for icons exclusively tied to `chrome/app` /
       desktop Chrome browser UI.
     - `//ash/resources/vector_icons/` for ChromeOS Ash-specific icons.
     - `//ui/views/vector_icons/` for core Views framework internals.
     - `//components/omnibox/browser/vector_icons/` for omnibox-only icons.
   - **Trademarked / branded logos:** Place Google-branded logos in
     `//components/vector_icons/google_chrome/` with a Chromium counterpart in
     `//components/vector_icons/chromium/`.

______________________________________________________________________

## Step 2: Fetch and Convert to `.icon`

Run [fetch_icon.py](scripts/fetch_icon.py) to download the Google Symbols SVG
(enforcing **100 ROND** and **400 weight**) and print the converted
`CANVAS_DIMENSIONS` block(s) to stdout:

- **`24px`**: Fetches `<icon>/rond100/24px.svg` (or
  `<icon>/fill1rond100/24px.svg` with `--fill`).
- **`20px`**: Fetches `<icon>/rond100/20px.svg` (or
  `<icon>/fill1rond100/20px.svg` with `--fill`).
- **`16px`**: Fetches `<icon>/wght500rond100/20px.svg` (or
  `<icon>/wght500fill1rond100/20px.svg` with `--fill`) and scales from
  `viewBox="0 -960 960 960"` to `16x16` (`16/20 == 400/500`, preserving the 400
  stroke weight at 16px).

```bash
# Single size (e.g. 20px outlined icon):
python3 agents/skills/add-vector-icon/scripts/fetch_icon.py \
  --icon "<gm3_icon_name>" \
  --sizes 20

# Multiple sizes (e.g. used at both 20px and 16px in the CL):
python3 agents/skills/add-vector-icon/scripts/fetch_icon.py \
  --icon "<gm3_icon_name>" \
  --sizes 20 16

# Filled variant (e.g. 24px filled icon):
python3 agents/skills/add-vector-icon/scripts/fetch_icon.py \
  --icon "<gm3_icon_name>" \
  --fill \
  --sizes 24
```

Write the resulting output into the target `.icon` file (e.g.
`components/vector_icons/<gm3_icon_name>.icon`) with a single Chromium BSD
copyright header at the top (or splice the new size block into an existing
`.icon` file in descending size order).

______________________________________________________________________

## Step 3: Register in `BUILD.gn` and Verify

1. **Register the `.icon` file in `BUILD.gn`:**
   - Open the corresponding `BUILD.gn` file (either in the same directory, such
     as `//components/vector_icons/BUILD.gn` or
     `//chrome/app/vector_icons/BUILD.gn`, or in the parent directory when
     `icon_directory = "vector_icons"`, such as `//ui/views/BUILD.gn`).
   - Add the `.icon` path (relative to `icon_directory`) in alphabetical order
     inside the `aggregate_vector_icons` `sources` array, keeping it within any
     `# go/keep-sorted` block or platform/branding conditionals
     (`is_chrome_branded`, `is_mac`, etc.).
2. **Inspect the generated `.icon` file:**
   - Starts with a single Chromium BSD copyright header.
   - Contains `CANVAS_DIMENSIONS, <size>,` blocks in **strictly descending
     order** of size (`24` -> `20` -> `16`).
   - Ends cleanly with `CLOSE` (no trailing comma).
3. **Compile check (optional):**
   - Build the corresponding vector icons target (e.g.
     `autoninja -C out/Default components/vector_icons` or
     `chrome/app/vector_icons`) to verify `aggregate_vector_icons.py` compiles
     the new `.icon` file into `$target_gen_dir/vector_icons.h` / `.cc` without
     errors.
