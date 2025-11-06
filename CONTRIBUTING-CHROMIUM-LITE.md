# Contributing to Chromium-Lite

Thank you for your interest in contributing to Chromium-Lite! This document provides guidelines for contributing to the project.

## 📋 Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Submitting Changes](#submitting-changes)
- [Code Style](#code-style)
- [Testing](#testing)
- [Documentation](#documentation)

## 📜 Code of Conduct

By participating in this project, you agree to:
- Be respectful and inclusive
- Accept constructive criticism gracefully
- Focus on what is best for the community
- Show empathy towards other community members

## 🚀 Getting Started

### Prerequisites

Before contributing, ensure you have:
- **Development Machine**: Linux with 100GB+ free space, 16GB+ RAM
- **Git**: Installed and configured
- **GitHub Account**: For submitting pull requests
- **Build Tools**: depot_tools and Chromium build dependencies

### Finding Issues

Good places to start:
- Issues labeled `good-first-issue`
- Issues labeled `help-wanted`
- Documentation improvements
- Bug reports

## 💻 Development Setup

### 1. Fork the Repository

```bash
# Fork on GitHub, then clone your fork
git clone https://github.com/YOUR_USERNAME/chromium-lite.git
cd chromium-lite

# Add upstream remote
git remote add upstream https://github.com/amzu-dev/chromium-lite.git
```

### 2. Set Up Build Environment

```bash
# Install depot_tools
cd ~
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
export PATH="$HOME/depot_tools:$PATH"

# Navigate to chromium-lite directory
cd chromium-lite

# Sync dependencies
gclient sync --no-history

# Install build dependencies (Ubuntu/Debian)
sudo apt-get install -y build-essential ninja-build pkg-config \
  libglib2.0-dev libgtk-3-dev libnss3-dev
```

### 3. Create a Development Branch

```bash
# Update your main branch
git checkout main
git pull upstream main

# Create a feature branch
git checkout -b feature/my-feature-name
```

## ✏️ Making Changes

### Code Organization

Key directories modified in Chromium-Lite:
- `chrome/browser/ui/tabs/` - Tab limiting functionality
- `chrome/browser/ui/browser_navigator.cc` - Navigation with tab limits
- `chrome/browser/ui/browser_command_controller.cc` - UI command handling
- `.github/workflows/` - CI/CD workflows

### Development Workflow

1. **Make Your Changes**
   ```bash
   # Edit files
   vim chrome/browser/ui/tabs/tab_limit_manager.cc
   ```

2. **Build and Test**
   ```bash
   # Configure build
   gn gen out/Debug

   # Build chrome
   ninja -C out/Debug chrome

   # Test your changes
   out/Debug/chrome
   ```

3. **Verify Tab Limits**
   - Try opening more than 4 regular tabs
   - Try opening more than 1 incognito tab
   - Verify UI elements are disabled appropriately

4. **Check for Plugin References**
   - Ensure no plugin code is accidentally reintroduced
   - Grep for plugin-related symbols

## 📤 Submitting Changes

### Before Submitting

- [ ] Code follows the Chromium style guide
- [ ] All changes are tested locally
- [ ] Commit messages are clear and descriptive
- [ ] No unnecessary files included
- [ ] Documentation updated if needed

### Commit Message Format

```
Short summary (50 chars or less)

More detailed explanation if necessary. Wrap at 72 characters.
Explain what and why, not how.

Fixes: #123
```

Examples:
```
Fix tab limit enforcement in incognito mode

The tab limit was not properly enforced when opening incognito
tabs through keyboard shortcuts. Updated the command controller
to check tab limits before creating new incognito tabs.

Fixes: #45
```

### Creating a Pull Request

1. **Push to Your Fork**
   ```bash
   git push origin feature/my-feature-name
   ```

2. **Open Pull Request**
   - Go to https://github.com/amzu-dev/chromium-lite
   - Click "New Pull Request"
   - Select your branch
   - Fill out the PR template

3. **PR Description Template**
   ```markdown
   ## Description
   Brief description of changes

   ## Type of Change
   - [ ] Bug fix
   - [ ] New feature
   - [ ] Breaking change
   - [ ] Documentation update

   ## Testing
   How was this tested?

   ## Screenshots (if applicable)

   ## Related Issues
   Fixes #(issue number)
   ```

### Review Process

- Maintainers will review your PR
- Address feedback promptly
- Keep discussion respectful and constructive
- CI must pass before merging

## 🎨 Code Style

### C++ Style

Follow the [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md):

```cpp
// Good
int CountTabs(Profile* profile) {
  int count = 0;
  for (Browser* browser : *BrowserList::GetInstance()) {
    count += browser->tab_strip_model()->count();
  }
  return count;
}

// Bad - inconsistent naming, spacing
int count_tabs(Profile *profile){
  int count=0;
  for(Browser* browser:*BrowserList::GetInstance()){
    count+=browser->tab_strip_model()->count();
  }
  return count;
}
```

### File Headers

Include copyright headers in new files:
```cpp
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
```

### Naming Conventions

- **Functions**: `PascalCase` or `CamelCase`
- **Variables**: `snake_case`
- **Constants**: `kConstantName`
- **Classes**: `PascalCase`
- **Namespaces**: `lowercase`

## 🧪 Testing

### Manual Testing

1. **Build Debug Version**
   ```bash
   gn gen out/Debug --args='is_debug=true'
   ninja -C out/Debug chrome
   ```

2. **Test Tab Limits**
   - Open 4 regular tabs
   - Attempt to open a 5th tab (should fail)
   - Open incognito window
   - Open 1 incognito tab
   - Attempt to open a 2nd incognito tab (should fail)

3. **Test UI**
   - Verify "New Tab" button disables at limit
   - Verify Ctrl+T does nothing at limit
   - Verify context menu items disabled at limit

### Automated Testing

Currently, automated tests are limited. Contributions to add tests are welcome!

## 📚 Documentation

### Update Documentation

When making changes, update:
- Code comments for complex logic
- README-CHROMIUM-LITE.md for user-facing changes
- This CONTRIBUTING.md for developer workflow changes
- Changelog for version updates

### Documentation Style

- Use clear, concise language
- Include code examples where helpful
- Keep line length to 80 characters in markdown
- Use proper markdown formatting

## 🏷️ Release Process

Releases are managed by maintainers:

1. Version bump in VERSION file
2. Update CHANGELOG.md
3. Create release tag
4. GitHub Actions builds artifacts
5. Create GitHub release

## ❓ Questions?

- **General Questions**: Open a GitHub Discussion
- **Bug Reports**: Open an issue with `bug` label
- **Feature Requests**: Open an issue with `enhancement` label
- **Security Issues**: Email maintainers privately (see README)

## 🙏 Thank You!

Your contributions make Chromium-Lite better for everyone. Thank you for taking the time to contribute!

---

**Remember**: Quality over quantity. Small, well-tested PRs are better than large, untested ones.
