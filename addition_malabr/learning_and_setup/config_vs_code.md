# Config VS Code

## 1. Install this vs code extension

1. C/C++
2. clangd
3. Container Tools
4. Dev Containers
5. GN Language Server
6. Mojom IDL - need `rust` and  `mojom-lsp` [link](https://github.com/GoogleChromeLabs/mojom-language-support/blob/main/mojom-lsp/README.md)
7. Toggle Header/Source
8. vscode-proto3
9. Bookmarks

### 2. VS Code with `compile_commands.json`

needed for autocomplete and definition navigation.

**1. Generate `compile_commands.json`**

Chromium uses GN + Ninja, so you must tell GN to export this file:

```bash
gn gen out/Default --export-compile-commands
```

This generates `out/Default/compile_commands.json`.

Then create a symlink in the root:

```bash
ln -s out/Default/compile_commands.json .
```

### 3. Update `.vscode/settings.json` in the codebase

```json
{
	"mojom.enableLanguageServer": "Enabled",
	"C_Cpp.default.compileCommands": "${workspaceFolder}/compile_commands.json",
	"clangd.arguments": ["--compile-commands-dir=."],
	"C_Cpp.intelliSenseEngine": "disabled"
}
```

