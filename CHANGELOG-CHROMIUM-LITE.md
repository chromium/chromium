# Changelog

All notable changes to Chromium-Lite will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Planned
- Automated testing infrastructure
- Windows and macOS builds
- Additional memory optimizations
- Performance benchmarks

## [1.0.0] - 2024-11-06

Initial release of Chromium-Lite with major modifications to the Chromium browser.

### Added
- Tab limiting system with configurable limits
  - Maximum 4 regular (non-incognito) tabs
  - Maximum 1 incognito tab
  - Multi-level enforcement (UI, navigation, model)
- Tab limit manager module (`chrome/browser/ui/tabs/tab_limit_manager.h/.cc`)
- GitHub Actions CI/CD workflows
  - Automated build workflow
  - Manual release workflow
  - Pull request checks
- Comprehensive documentation
  - README-CHROMIUM-LITE.md
  - CONTRIBUTING-CHROMIUM-LITE.md
  - This CHANGELOG

### Removed
- Complete plugin system removal (152 files, 14,679 lines)
  - Removed `chrome/browser/plugins/` directory
  - Removed `chrome/renderer/plugins/` directory
  - Removed `components/plugins/` directory
  - Removed `content/browser/plugin_*` files
  - Removed `content/public/browser/plugin_*` files
  - Removed `content/public/common/webplugininfo*` files
  - Removed `chrome/common/plugin.mojom`
  - Removed Blink plugin interfaces
  - Removed `third_party/blink/renderer/modules/plugins/`
  - Removed plugin-related test files
- PDF viewer functionality (depends on plugin system)
- Print preview (depends on plugin system)
- Plugin test utilities

### Changed
- Set `enable_plugins = false` in `content/public/common/features.gni`
- Modified `browser_navigator.cc` to enforce tab limits before tab creation
- Modified `tab_strip_model.cc` to add secondary tab limit enforcement
- Modified `browser_command_controller.cc` to disable "New Tab" UI when limit reached
- Updated BUILD.gn files to remove plugin references
- Optimized build configuration for size and performance

### Security
- Reduced attack surface by removing plugin system
- Eliminated NPAPI/PPAPI-related vulnerabilities
- Removed complex legacy plugin code

### Performance
- Smaller binary size (estimated ~25% reduction)
- Lower memory usage with tab limiting
- Removed unnecessary plugin infrastructure

## Version History

### Pre-release Development

Prior to v1.0.0, this was a standard Chromium clone. The following commits mark the beginning of Chromium-Lite:

- **Commit 8711333493**: Remove plugins feature from Chromium
- **Commit 6de4480465**: Limit tabs to 4 regular tabs + 1 incognito tab

## Migration Notes

### From Standard Chromium

If migrating from standard Chromium:

1. **Plugins Not Supported**: Any workflow relying on plugins (including PDF viewing) will need alternatives
2. **Tab Limits**: Users accustomed to many tabs will be limited to 4 regular + 1 incognito
3. **Extensions**: May not work if they rely on plugin APIs (to be verified)

### Configuration Changes

- `enable_plugins` flag: Now hardcoded to `false`
- Tab limits: Defined in `tab_limit_manager.h` as constants
  - `kMaxRegularTabs = 4`
  - `kMaxIncognitoTabs = 1`

## Known Issues

### v1.0.0

- [ ] No automated tests for tab limiting functionality
- [ ] Build artifacts may be large (>100MB)
- [ ] Build time is significant (2-4+ hours)
- [ ] GitHub Actions builds may timeout on complex builds

## Upgrade Path

### To Future Versions

When upgrading between Chromium-Lite versions:
1. Download new release
2. Extract to new directory
3. Migrate user data if needed (typically in `~/.config/chromium-lite`)
4. Remove old version

### Build from Source

To build a specific version:
```bash
git checkout v1.0.0
# Follow build instructions in README-CHROMIUM-LITE.md
```

## Deprecation Notices

### Deprecated in v1.0.0
- Plugin API (removed)
- PPAPI/NPAPI support (removed)

### Planned Deprecations
- None currently

## Acknowledgments

- Original Chromium project and contributors
- Chromium security team for plugin deprecation guidance

## Support and Feedback

- **Issues**: https://github.com/amzu-dev/chromium-lite/issues
- **Discussions**: https://github.com/amzu-dev/chromium-lite/discussions
- **Pull Requests**: https://github.com/amzu-dev/chromium-lite/pulls

---

**Note**: This changelog covers changes specific to Chromium-Lite. For upstream Chromium changes, see the [official Chromium release notes](https://chromium.googlesource.com/chromium/src/+/main/docs/branch_notes.md).
