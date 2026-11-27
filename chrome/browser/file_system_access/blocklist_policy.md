# File System Access API Blocklist Policy

This document describes the scope and rationale behind Chrome's File System
Access (FSA) API sensitive-entry blocklist and filename restrictions, and draws
a clear boundary for what is in scope vs. out of scope.

The implementation lives in:

- Path blocklist:
  [`chrome/browser/file_system_access/chrome_file_system_access_permission_context.cc`](chrome_file_system_access_permission_context.cc)
- Filename and extension sanitization:
  [`content/browser/file_system_access/file_system_access_manager_impl.cc`](../../../content/browser/file_system_access/file_system_access_manager_impl.cc)
  and
  [`content/browser/file_system_access/file_system_chooser.cc`](../../../content/browser/file_system_access/file_system_chooser.cc)

## The Policy Line

> Chrome's File System Access blocklist protects OS/browser directories,
> user-profile configuration, ambient OS file-manager triggers, and widely used
> version-control metadata that can execute commands during routine file
> browsing or read-only inspection. It does not attempt to enumerate
> per-directory configuration files that only take effect when the user
> explicitly builds, runs, or opens a project in a developer tool or third-party
> application.

The blocklist is a defense-in-depth layer to prevent users from *accidentally*
exposing critical OS/profile state or planting passive execution triggers inside
content-agnostic folders (folders holding documents, notes, media, or versioned
plain text). It is not a general-purpose sandbox around arbitrary local
software once a user explicitly grants a site access to a directory.

## What Is Blocked (and Why)

### Broad Personal Root Directories (Parent Blocked, Children Allowed)

- **Paths**: Home (`DIR_HOME`), Desktop, Documents, and Downloads
  (`kDontBlockChildren`), plus Windows Temporary Internet Files for MTP device
  access (`kBlockNestedDirectories`).
- **Rationale**: Prevents users from accidentally granting a site access to
  their entire personal directory tree in one click, while still allowing them
  to select specific subdirectories or files inside.

### Operating System, Browser, and User-Profile Directories (Read and Write Blocked)

- **OS and application directories**: Windows (`Windows`, `Program Files`),
  macOS (`/System/Volumes`, `/Applications`, `~/Applications`, `/dev`), Linux,
  ChromeOS, and Android (`/dev`, `/proc`, `/sys`, `/boot`, `/etc`, `/run`), and
  Chrome's own installation and profile directories (`DIR_EXE`, `DIR_MODULE`,
  `DIR_ASSETS`, `DIR_USER_DATA`, `DIR_OUTER_BUNDLE`).
- **User-profile configuration and credentials**: All direct dot-files and
  dot-directories under the home directory (`DIR_HOME/.*`, including `~/.ssh`,
  `~/.gnupg`, `~/.config`, `~/.cache`, `~/.pki`, `~/.dbus`, `~/.local/share`,
  shell startup scripts, and Vim startup files), Windows non-dot Vim config
  files (`vimfiles`, `_vimrc`, `_gvimrc`, `_exrc`), Windows `AppData` and
  `ProgramData`, macOS `~/Library` (with narrow carve-outs for `CloudStorage`,
  `Containers`, and iCloud `Mobile Documents`), Linux/ChromeOS/Android
  `DIR_CACHE`, and Android app data.
- **Windows local/hypervisor/RDP UNC paths and admin shares**: Blocked to
  prevent bypassing local path rules via loopback or share aliases (with a
  carve-out for WSL home paths).
- **Rationale**: Exposure of OS binaries, browser profiles, credentials, or
  global login/shell startup files compromises the entire host regardless of
  which folder the user intended to share.

### OS File-Manager Passive Triggers (Filename and Extension Restrictions)

- **Blocked names and extensions**: `.lnk`, `.scf`, `.url`, `.library-ms`,
  `.searchConnector-ms`, Windows `DownloadFileType::DANGEROUS` extensions such
  as `.ini` (`desktop.ini`) and `.dll` (with `.local` exempted for
  programmatic writes), CLSID extensions (`*.{...}`), Windows reserved device
  names, and illegal/trailing-dot/space names.
- **Rationale**: Merely opening or viewing a folder in an OS file manager such
  as Windows Explorer can passively parse these files, hijack DLLs, or trigger
  remote SMB/NTLM requests without the user executing anything.

### Widely Used Version-Control Metadata (Write Blocked, Read Allowed)

- **Blocked directories**: `*/.git`, `*/.hg` (Mercurial), and `*/.jj` (Jujutsu)
  using `kBlockWrite`.
  - For `.git`, Chrome carves out a narrow write allowlist for safe repository
    data, ref, and index paths (`objects` excluding `objects/info`, `refs`,
    `reftable`, `logs`, `HEAD`, `index`, `packed-refs`, etc.) so browser-based
    Git clients such as `isomorphic-git` continue to work, while keeping
    `.git` creation/gitfiles, `config`, `hooks`, `modules`, `commondir`, and
    `worktrees` write-blocked.
  - For `.hg` and `.jj`, all writes are blocked with no inner allowlist because
    there is no mainstream browser-side client ecosystem writing directly into
    those directories over FSA.
- **Rationale**: Version-control systems are content-agnostic tools commonly
  used for plain-text notes, documents, and assets as well as software. Routine,
  read-only inspection (`git status`, `hg status`, `jj log`, or shell prompt
  branch indicators) is not expected to run code, yet VCS metadata (`.git/config`,
  `.git/hooks`, `.hg/hgrc`, legacy `.jj/repo/config.toml`, and non-colocated
  `.jj/repo/store/git/`) can trigger arbitrary command execution. Because files
  written through FSA are owned by the current OS user, they also bypass
  owner-based trust checks such as Git's `safe.directory` and Mercurial's
  `[trusted]`.

## What Is NOT Blocked (Out of Scope)

Once a user explicitly grants access to a directory, Chrome does **not** block
per-directory configuration files inside that directory for the following
categories:

- **Build tools, package managers, and project scripts** (for example, `.mvn`,
  `.cargo`, MSBuild `Directory.Build.targets`, `package.json`, `Makefile`,
  `pyproject.toml`, `build.gradle`, `flake.nix`): These operate on software
  source code and only execute when the user explicitly runs a build or package
  command. Attempting to enumerate build tools creates an unbounded blocklist
  and breaks web-based IDEs and code editors.
- **Third-party editors, IDEs, note-taking apps, and shell auto-loaders** (for
  example, `.vscode/`, `.idea/`, `.obsidian/`, `.envrc`, `mise.toml`,
  `.dir-locals.el`, `.nvim.lua`, `.vimrc`/`.exrc`, `.Rprofile`): Modern tools
  enforce their own workspace-trust prompts, content-hash allowlists, or
  restricted modes on fresh folders, or require explicit opt-in or tool launch.
  Enforcing per-workspace trust or integrity checks for project files is the
  responsibility of the local application, not the browser.
- **Niche or legacy VCS tools and non-executing tracked repo files** (for
  example, `.svn/`, `.gitattributes`, `.gitmodules`, `.lfsconfig`, repo-root
  `.gitconfig`, Sapling `.sl`, Darcs `_darcs`, Bazaar `.bzr`, Fossil
  `_FOSSIL_`, CVS `CVS/`, Perforce `P4CONFIG`): These either do not execute
  commands by design, have minimal adoption, collide with ordinary folder names
  on case-insensitive filesystems, or rely on user-configurable filenames.
- **Folder-sync and backup tools** (for example, Syncthing, Rclone, Dropbox,
  OneDrive, iCloud, Borg, Restic): These do not execute arbitrary commands from
  per-directory metadata; their user-profile configurations are already covered
  by Chrome's home-directory dotfile and OS app-data rules.

## Evaluating Future Proposals

A new entry should only be considered for the blocklist if it meets **all** of
the following criteria:

- **Passive or read-only trigger in content-agnostic folders**: Writing the
  entry enables code execution, credential leakage, or host compromise during
  routine OS file browsing, login/startup, or read-only inspection of ordinary
  folders—without the user explicitly building or running a software project.
- **No tool-owned trust boundary**: The target tool or OS does not already gate
  untrusted folders behind its own trust prompt or integrity check (or FSA's
  user-owned writes inherently bypass an ownership-only check).
- **High reach and low breakage**: The target affects a broad set of users and
  blocking it will not break legitimate web applications (such as web IDEs or
  editors) editing ordinary project files.
