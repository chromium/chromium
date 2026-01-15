# Agent Browser Protocol - Chromium Fork

This is a Chromium fork implementing the Agent Browser Protocol (ABP) - a REST-based API for AI agent browser control at the C++ engine level.

## Project Documentation

See `plans/` for technical design specifications:
- `plans/agent-browser-protocol.md` - Core ABP architecture
- `plans/API.md` - Complete REST API specification
- `plans/mcp.md` - MCP server for AI agent integration

## Build Setup

### Prerequisites

1. **depot_tools** (Google's build toolchain):
   ```bash
   git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git ~/depot_tools
   echo 'export PATH="$HOME/depot_tools:$PATH"' >> ~/.bashrc
   source ~/.bashrc
   ```

2. **Build dependencies** (Ubuntu/Debian):
   ```bash
   sudo ./build/install-build-deps.sh --no-prompt
   ```

### Directory Structure

```
/home/paladin/src/
├── .gclient           # gclient configuration
├── src -> chromium    # symlink required by gclient
└── chromium/          # source code
    ├── out/Default/   # build output
    └── plans/         # ABP design docs
```

### Sync Dependencies

```bash
cd /home/paladin/src
gclient sync --no-history
```

### Configure Build

Debug component build (faster incremental builds):
```bash
cd /home/paladin/src/src
gn gen out/Default --args='is_debug=true is_component_build=true symbol_level=1 dcheck_always_on=true'
```

### Build Chromium

```bash
cd /home/paladin/src/src
autoninja -C out/Default chrome
```

First build: ~4-6 hours. Incremental builds: seconds to minutes.

### Run

```bash
./out/Default/chrome
```

## Development Notes

- Source is at `/home/paladin/src/chromium` (symlinked as `src` for gclient)
- Build output at `out/Default/`
- Use `autoninja` (not `ninja`) for automatic parallelism
- Run `gclient sync` after pulling changes to update dependencies
