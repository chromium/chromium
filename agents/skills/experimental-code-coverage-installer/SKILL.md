---
name: experimental-code-coverage-installer
description: >-
  Verifies and initializes the Chromium development environment and dependencies
  required for running code coverage tools and services.
---

# Code Coverage Installer Skill

This skill guides the verification and initialization of the development
environment.

______________________________________________________________________

## When to Use This Skill

Use this skill when:

- Preparing the workspace for running code coverage pipelines, local generation,
  or triage.
- Verifying the presence and integrity of code coverage tools and code
  repositories.

______________________________________________________________________

## How to Use This Skill

The primary and recommended method to verify and initialize all code coverage
dependencies automatically is to run the installer script with the required
`--infra-dir` argument:

```bash
python3 tools/code_coverage/code_coverage_installer.py --infra-dir <path_to_infra_dir>
```

> [!IMPORTANT] If the user has not explicitly provided an infra directory path
> in their request, you **must** ask them to specify one before running the
> installer or starting manual verification. Do not choose a default directory
> or initialize the repository without the user's explicit consent.

If you prefer to perform checks manually, or if you need to troubleshoot
specific components, follow the manual checklist workflow below:

### Step 1: Verify Chromium Checkout on Disk

Before performing any code coverage operations, verify that a valid Chromium
source code checkout is available on the local disk.

1. **Locate the Workspace Root:** Confirm you are inside or pointing to the root
   of a Chromium checkout (typically containing the `src/` directory).
2. **Verify Vital Files:** Check for the presence of the following critical
   files and directories to ensure the checkout is valid:
   - `src/DEPS`: Contains Chromium's external dependency definitions.
   - `src/tools/code_coverage/`: The root directory for coverage-related
     scripts.
3. **Verdict:** If these files/directories are not present, stop and notify the
   user. A valid local Chromium checkout is strictly required.

### Step 2: Verify LLVM Coverage Tools

Verify that LLVM coverage tools are available in `third_party/`.

1. **Check for Existing Tooling:** Check if the following executables exist:
   - `src/third_party/llvm-build/Release+Asserts/bin/llvm-cov`
   - `src/third_party/llvm-build/Release+Asserts/bin/llvm-profdata`
2. **If Tools are Present:** Proceed to the next step.
3. **If Tools are Missing:** Prompt the user for confirmation before modifying
   `.gclient` (as it will affect future sync speeds).
   - If they confirm, configure `.gclient` so that coverage tools are fetched
     during workspace syncs:
     1. Open the `.gclient` configuration file (located in the directory above
        `src/`).
     2. Add the following entry to the `custom_vars` dictionary:
        ```python
        "checkout_clang_coverage_tools": True,
        ```
     3. Run hooks to download the package:
        ```bash
        gclient runhooks
        ```
   - If they do not confirm, stop and notify the user that LLVM tools are
     required for coverage verification.
4. **Verdict:** If the tools are still missing, stop and notify the user.

### Step 3: Verify and Setup Code Coverage Recipe Code

Verify that the code coverage recipe codebase is available under the specified
`<infra_dir>`. This code runs on LUCI builders to generate raw coverage data and
upload it to GCS.

1. **Check for Existing Recipes Checkout:** Check if the following directory
   exists under your `<infra_dir>`:
   - `<infra_dir>/build/recipes/recipe_modules/code_coverage/`
2. **Verify Key Recipe Files:** Confirm that the following files exist inside
   `code_coverage/`:
   - `api.py`: Contains the recipe module API.
   - `constants.py`: Contains configuration constants.
   - `properties.proto`: Defines configuration properties.
3. **If Recipe Code is Present:** Proceed to the next step.
4. **If Recipe Code is Missing:** Follow the setup instructions in Step 5.
5. **Verdict:** If the recipe repository is missing after setup, stop and notify
   the user.

### Step 4: Verify and Setup Code Coverage Service Code

Verify that the code coverage service codebase is available under the specified
`<infra_dir>`. This is the AppEngine service (Findit) that ingests, processes,
and serves the coverage data to Gerrit and the dashboard.

1. **Check for Existing Service Checkout:** Check if the following directory
   exists under your `<infra_dir>`:
   - `<infra_dir>/infra/appengine/findit/`
2. **Verify Key Service Files:** Confirm that the following files exist inside
   `findit/`:
   - `app.yaml`: AppEngine configuration.
   - `handlers/code_coverage/`: Directory containing handlers for coverage APIs.
   - `services/code_coverage/`: Service logic for processing code coverage.
3. **If Service Code is Present:** The environment setup is complete.
4. **If Service Code is Missing:** Follow the setup instructions in Step 5.
5. **Verdict:** If the service repository is missing after setup, stop and
   notify the user.

### Step 5: Setup Chrome Infra Superproject

If either the recipe code or service code is missing, they must be checked out
together using the `infra_superproject` configuration.

1. **Create the Target Directory:** If the `<infra_dir>` (e.g.,
   `~/chrome_infra`) does not exist, create it.
2. **Initialize Checkout:**
   - If the directory is already initialized (contains `.gclient`), run:
     ```bash
     cd <infra_dir>
     gclient sync
     ```
   - If it is a new directory, run:
     ```bash
     cd <infra_dir>
     fetch infra_superproject
     ```

### Step 6: Verify Global Coverage Agent Rules in `GEMINI.md`

Check that your local `//GEMINI.md` file imports the global code coverage agent template:
1. Check if `GEMINI.md` exists in the repository root.
2. Verify that it contains:
   ```markdown
   @agents/prompts/templates/code_coverage.md
   ```
3. If missing, append `@agents/prompts/templates/code_coverage.md` so all code coverage agents and subagents automatically inherit infrastructure constraints (e.g., prohibiting web page scraping on LUCI/Swarming).
