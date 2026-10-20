# Chromium Translation Screenshots SKILL

A skill that guides agents in identifying new or updated translatable strings in
a Chromium change (`.grd` or `.grdp` files), capturing appropriate UI
screenshots with sufficient surrounding context, and uploading them to Google
Cloud Storage via `tools/translation/upload_screenshots.py` to generate and
stage `.png.sha1` hash files.

## Setup

Agents that discover skills in `.agents/skills` (such as Gemini CLI) can link
this skill using the setup script:

```bash
python3 agents/skills/setup.py link translation-screenshots
```

For Antigravity, ensure `agents/skills` is registered in `.agents/skills.json`.

## Usage

The skill activates when modifying `.grd` or `.grdp` resource files or when
adding/updating user-facing UI strings:

- "Identify translatable strings requiring screenshots in this change"
- "Capture and upload a translation screenshot for IDS_SETTINGS_FOO"
- "Stage translation screenshot hashes and verify presubmit"

## File Structure

```
agents/skills/translation-screenshots/
├── OWNERS                      # Code ownership
├── SKILL.md                    # SKILL definition (source)
└── README.md                   # This file
```

To use this skill, symlink or copy `SKILL.md` to your agent's skills directory
(e.g., `.agents/skills/translation-screenshots/` or via
`python3 agents/skills/setup.py link translation-screenshots`).

## Workflow

1. **Identify Translatable Strings**: Inspect `.grd` and `.grdp` changes for
   translatable messages (messages without `translateable="false"`).
2. **Capture UI Screenshots**: Frame the target control with sufficient
   surrounding context, matching the `.grd` copy exactly.
3. **Upload to Cloud Storage**: Run
   `python3 tools/translation/upload_screenshots.py` to upload the screenshot to
   `gs://chromium-translation-screenshots` and generate the `.png.sha1` file.
4. **View Uploaded Screenshots**: Provide viewing links to the user if they are
   a Google employee using `http://go/translation-hash/<sha1>`.
5. **Git Hygiene**: Stage the `.png.sha1` file and delete the raw `.png` image.

## Verification

Validate the skill file specification:

```bash
python3 third_party/depot_tools/agents/skills/skill-validator/scripts/skill_validator.py agents/skills/translation-screenshots/SKILL.md
```
