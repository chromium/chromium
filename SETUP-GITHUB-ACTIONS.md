# Setting Up GitHub Actions for Chromium-Lite

GitHub Actions workflows have been prepared but require special permissions to be pushed via git. Follow these instructions to add them to your repository.

## Why This Is Needed

GitHub restricts workflow file changes via GitHub Apps for security reasons. The workflows need to be added either:
1. Through the GitHub web interface (recommended), or
2. By configuring workflow permissions first, then pushing

## Method 1: Add Workflows Through GitHub Web Interface (Recommended)

### Step 1: Enable Workflow Permissions

1. Go to your repository: https://github.com/amzu-dev/chromium-lite
2. Click **Settings** tab
3. In the left sidebar, click **Actions** → **General**
4. Scroll to **Workflow permissions**
5. Select **"Read and write permissions"**
6. Check **"Allow GitHub Actions to create and approve pull requests"**
7. Click **Save**

### Step 2: Create Workflow Directory

1. In your repository on GitHub, navigate to the root
2. Click **Add file** → **Create new file**
3. Type `.github/workflows/build-chromium-lite.yml` in the filename field
4. This will automatically create the `.github/workflows/` directory

### Step 3: Add build-chromium-lite.yml

Copy the contents from `.github/workflows/build-chromium-lite.yml` on your local machine and paste into the GitHub editor, then commit.

### Step 4: Add manual-release.yml

1. In the `.github/workflows/` directory on GitHub
2. Click **Add file** → **Create new file**
3. Name it `manual-release.yml`
4. Copy contents from local file and paste
5. Commit the file

### Step 5: Add pr-checks.yml

Repeat the same process for `pr-checks.yml`.

## Method 2: Push Workflows via Git (After Permission Fix)

If you've configured workflow permissions in Step 1 above:

```bash
# Navigate to your chromium-lite directory
cd /home/user/chromium

# Add workflow files
git add .github/workflows/

# Commit workflows
git commit -m "Add GitHub Actions workflows for automated builds"

# Push to remote
git push origin claude/remove-plugins-feature-011CUrQNM8Ngaibj8Qc9Zikn
```

## Verifying Workflows Are Active

Once workflows are added:

1. Go to **Actions** tab in your GitHub repository
2. You should see three workflows listed:
   - Build Chromium-Lite
   - Manual Release
   - Pull Request Checks

3. Click on any workflow to see its configuration

## Triggering Your First Build

### Automatic Build

Push any change to the main branch or create a pull request, and the build will trigger automatically.

### Manual Build

1. Go to **Actions** tab
2. Select **"Build Chromium-Lite"** from the left sidebar
3. Click **"Run workflow"** button
4. Select branch and build type (Release/Debug)
5. Click **"Run workflow"**

The build will start and you can monitor progress in the Actions tab.

## Expected Build Time

- **First build**: 3-6 hours (downloads dependencies, builds from scratch)
- **Subsequent builds**: 1-3 hours (with caching)

## Build Artifacts

After a successful build:

1. Go to the completed workflow run
2. Scroll to **Artifacts** section at the bottom
3. Download the `chromium-lite-Release-linux-x64` artifact
4. Extract and test the browser

## Creating a Release

### Manual Release

1. Go to **Actions** tab
2. Select **"Manual Release"** workflow
3. Click **"Run workflow"**
4. Enter version (e.g., `1.0.0`)
5. Select build type
6. Choose whether to mark as pre-release
7. Click **"Run workflow"**

This will:
- Trigger a build
- Create a git tag
- Create a GitHub release
- Generate release notes

### Automatic Release

Builds from the main branch automatically create pre-release versions.

## Troubleshooting

### Workflow Permission Error

```
refusing to allow a GitHub App to create or update workflow
```

**Solution**: Complete Step 1 in Method 1 above.

### Build Timeout

If builds timeout after 6 hours:

**Solution**:
- Reduce build parallelism
- Disable some features in GN args
- Use a self-hosted runner with more resources

### Out of Disk Space

```
No space left on device
```

**Solution**: The workflow includes disk cleanup steps, but if it still fails:
- Reduce cache size
- Build fewer targets
- Use incremental builds

## Workflow Files

The three workflow files in `.github/workflows/`:

1. **build-chromium-lite.yml** (295 lines)
   - Automated builds on push/PR
   - Artifact generation
   - Automatic releases

2. **manual-release.yml** (141 lines)
   - Manual release creation
   - Version tagging
   - Release notes generation

3. **pr-checks.yml** (199 lines)
   - Code quality validation
   - Plugin reference detection
   - Build file syntax checking

## Next Steps

After workflows are set up:

1. ✅ Workflows appear in Actions tab
2. ✅ Trigger a test build
3. ✅ Monitor build progress
4. ✅ Download and test artifacts
5. ✅ Create your first release

## Support

If you encounter issues:
- Check workflow logs in the Actions tab
- Review this guide
- Check GitHub Actions documentation
- Open an issue in the repository

---

**Note**: Chromium builds are resource-intensive. Expect long build times and ensure sufficient GitHub Actions minutes are available in your account.
