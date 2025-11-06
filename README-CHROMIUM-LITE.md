# Chromium-Lite

A lightweight, streamlined version of Chromium with reduced features for improved security and resource efficiency.

[![Build Status](https://github.com/amzu-dev/chromium-lite/actions/workflows/build-chromium-lite.yml/badge.svg)](https://github.com/amzu-dev/chromium-lite/actions)
[![Release](https://img.shields.io/github/v/release/amzu-dev/chromium-lite?include_prereleases)](https://github.com/amzu-dev/chromium-lite/releases)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)

## Overview

Chromium-Lite is a modified version of the Chromium browser with the following key differences:

### 🎯 Key Features

- **No Plugin Support**: Complete removal of the plugin system (NPAPI/PPAPI)
  - Reduced attack surface
  - Smaller binary size
  - Improved security

- **Tab Limiting**: Built-in tab management
  - Maximum 4 regular tabs
  - Maximum 1 incognito tab
  - Helps reduce memory consumption

- **Optimized Build**: Compiled with size and performance optimizations
  - Minimal symbol information
  - Disabled unnecessary features
  - Component build disabled

## 📥 Download

Pre-built binaries are available on the [Releases](https://github.com/amzu-dev/chromium-lite/releases) page.

### Latest Release

Download the latest build:
- [Linux x64](https://github.com/amzu-dev/chromium-lite/releases/latest)

## 🚀 Quick Start

### Linux

```bash
# Download the latest release
wget https://github.com/amzu-dev/chromium-lite/releases/download/VERSION/chromium-lite-Release-linux-x64.tar.gz

# Verify checksum (recommended)
wget https://github.com/amzu-dev/chromium-lite/releases/download/VERSION/checksums.txt
sha256sum -c checksums.txt

# Extract
tar -xzf chromium-lite-Release-linux-x64.tar.gz
cd chromium-lite

# Run
./chrome
```

## 🔧 Building from Source

### Prerequisites

- **OS**: Ubuntu 20.04+ or compatible Linux distribution
- **Disk Space**: 100GB+ free space
- **RAM**: 16GB+ recommended
- **CPU**: Multi-core processor (build time scales with cores)

### Dependencies

```bash
# Install build dependencies
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  curl \
  git \
  lsb-release \
  ninja-build \
  pkg-config \
  python3 \
  python3-pip \
  sudo \
  wget

# Install Chromium-specific dependencies
sudo apt-get install -y \
  libasound2-dev \
  libatk-bridge2.0-dev \
  libatk1.0-dev \
  libcups2-dev \
  libdbus-1-dev \
  libdrm-dev \
  libgbm-dev \
  libglib2.0-dev \
  libgtk-3-dev \
  libnspr4-dev \
  libnss3-dev \
  libpango1.0-dev \
  libudev-dev \
  libx11-xcb-dev \
  libxcomposite-dev \
  libxcursor-dev \
  libxdamage-dev \
  libxrandr-dev \
  libxss-dev \
  libxtst-dev
```

### Build Steps

```bash
# 1. Install depot_tools
cd ~
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$HOME/depot_tools:$PATH"

# 2. Clone chromium-lite
git clone https://github.com/amzu-dev/chromium-lite.git
cd chromium-lite

# 3. Sync dependencies
gclient config --name=src --unmanaged https://github.com/amzu-dev/chromium-lite.git
gclient sync --no-history

# 4. Configure build
mkdir -p out/Release
cat > out/Release/args.gn << 'EOF'
is_debug = false
is_component_build = false
is_official_build = true
enable_nacl = false
enable_plugins = false
symbol_level = 0
optimize_for_size = true
target_cpu = "x64"
target_os = "linux"
EOF

# 5. Generate build files
gn gen out/Release

# 6. Build (this will take several hours)
ninja -C out/Release chrome

# 7. Run
out/Release/chrome
```

## 📊 Comparison with Standard Chromium

| Feature | Standard Chromium | Chromium-Lite |
|---------|------------------|---------------|
| Plugin Support | ✅ Yes | ❌ No |
| Tab Limit | ♾️ Unlimited | 4 regular + 1 incognito |
| PDF Viewer | ✅ Yes | ❌ No |
| Print Preview | ✅ Yes | ❌ No |
| WebRTC | ✅ Yes | ❌ No |
| Binary Size | ~200MB | ~150MB (estimated) |
| Memory Usage | Higher | Lower |

## 🛡️ Security Considerations

Chromium-Lite removes the plugin system entirely, which:

- ✅ Eliminates plugin-based vulnerabilities
- ✅ Reduces attack surface
- ✅ Removes complex legacy code (NPAPI/PPAPI)
- ⚠️ Disables PDF viewing in-browser (use external viewer)

## 🐛 Known Limitations

1. **No Plugin Support**: Extensions and plugins are not supported
2. **Limited Tabs**: Maximum 4 regular tabs and 1 incognito tab
3. **No PDF Viewer**: PDFs must be viewed with external applications
4. **No Print Preview**: Direct printing only
5. **No WebRTC**: Video conferencing features disabled

## 🤝 Contributing

Contributions are welcome! Please see [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.

### Development

```bash
# Create a feature branch
git checkout -b feature/my-feature

# Make changes
# ...

# Commit with descriptive messages
git commit -m "Add feature: description"

# Push and create PR
git push origin feature/my-feature
```

## 📝 Changelog

See [CHANGELOG.md](CHANGELOG.md) for version history and changes.

### Recent Changes

- **v1.0.0** (Latest)
  - Removed plugin system completely
  - Implemented tab limiting (4 regular + 1 incognito)
  - Optimized build for size and performance

## 📜 License

Chromium-Lite is based on the Chromium project and is licensed under the BSD-3-Clause license. See [LICENSE](LICENSE) for details.

Original Chromium copyright:
```
Copyright (c) 2024 The Chromium Authors. All rights reserved.
```

Chromium-Lite modifications:
```
Copyright (c) 2024 Chromium-Lite Contributors
```

## 🔗 Links

- **Homepage**: https://github.com/amzu-dev/chromium-lite
- **Issue Tracker**: https://github.com/amzu-dev/chromium-lite/issues
- **Releases**: https://github.com/amzu-dev/chromium-lite/releases
- **Original Chromium**: https://www.chromium.org/

## ⚙️ CI/CD

This project uses GitHub Actions for automated builds:

- **Build Workflow**: Automatically builds on push to main branches
- **Manual Release**: Trigger releases via GitHub Actions UI
- **Artifact Retention**: Build artifacts kept for 30 days

## 💬 Support

For questions and support:
- Open an issue on GitHub
- Check existing issues for solutions
- Review documentation in the repo

## 🙏 Acknowledgments

- The Chromium project and all contributors
- Google and the Chromium team for the original browser
- Open source community

---

**Note**: This is an unofficial fork of Chromium with specific modifications. It is not affiliated with Google or the Chromium project.
