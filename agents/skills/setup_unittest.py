#!/usr/bin/env vpython3

# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Tests for skills setup."""

import argparse
import io
import os
import subprocess
import unittest
import unittest.mock
from pathlib import Path

from pyfakefs import fake_filesystem_unittest

import setup


class TestSkillsSetup(fake_filesystem_unittest.TestCase):
    """Unit tests for skills setup functions."""

    # pylint: disable=protected-access

    def setUp(self):
        self.setUpPyfakefs()
        self.mock_run = unittest.mock.patch('subprocess.run').start()
        self.mock_get_cmd = unittest.mock.patch(
            'setup._get_gemini_cmd').start()
        self.mock_get_cmd.return_value = ['/path/to/gemini']
        self.addCleanup(unittest.mock.patch.stopall)

        self.gemini_cmd = ['/path/to/gemini']
        self.skill_path = Path('agents/skills/test-skill')

    def test_get_installed_skills(self):
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout=("skill-1 [Enabled]\n"
                    "  Description: desc 1\n"
                    "  Location: loc1/SKILL.md\n"
                    "skill-2 [Disabled]\n"
                    "  Description: desc 2\n"
                    "  Location: loc2/SKILL.md\n"),
            returncode=0)
        skills = setup.get_installed_skills()
        self.assertEqual(len(skills), 2)
        self.assertTrue(skills['skill-1'].enabled)
        self.assertFalse(skills['skill-2'].enabled)
        self.assertEqual(skills['skill-1'].location, 'loc1')
        self.assertTrue(skills['skill-1'].installed)
        self.mock_run.assert_called_once_with(
            ['/path/to/gemini', 'skills', 'list', '--debug'],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=True)

    def test_get_available_skills(self):
        project_root = Path('/root')
        skill1_dir = project_root / 'agents' / 'skills' / 'skill1'
        self.fs.create_dir(skill1_dir)
        self.fs.create_file(skill1_dir / 'SKILL.md')

        skill2_dir = project_root / 'internal' / 'agents' / 'skills' / 'skill2'
        self.fs.create_dir(skill2_dir)
        self.fs.create_file(skill2_dir / 'SKILL.md')

        # Not a skill (no SKILL.md)
        self.fs.create_dir(project_root / 'agents' / 'skills' / 'not-a-skill')

        with unittest.mock.patch('setup.SKILL_DIRS', [
                project_root / 'agents' / 'skills',
                project_root / 'internal' / 'agents' / 'skills'
        ]):
            available = setup.get_available_skills()

        self.assertEqual(len(available), 2)
        names = set(available)
        self.assertEqual(names, {'skill1', 'skill2'})
        for skill in available.values():
            self.assertIsInstance(skill, setup.SkillInfo)
            self.assertTrue(skill.available)
            self.assertTrue(skill.path.exists())

    def test_shorten_path(self):
        # Mock _PROJECT_ROOT to a fixed path for testing.
        with unittest.mock.patch('setup._PROJECT_ROOT', Path('/root/src')):
            # Case 1: Under project root
            self.assertEqual(setup._shorten_path('/root/src/foo/bar'),
                             '//foo/bar')

            # Case 2: Under user home
            with unittest.mock.patch('pathlib.Path.home',
                                     return_value=Path('/home/user')):
                self.assertEqual(setup._shorten_path('/home/user/docs'),
                                 '~/docs')

            # Case 3: Absolute path elsewhere
            self.assertEqual(setup._shorten_path('/other/path'), '/other/path')

            # Case 4: None
            self.assertEqual(setup._shorten_path(None), '-')

            # Case 5: Path.home() failure
            with unittest.mock.patch('pathlib.Path.home',
                                     side_effect=RuntimeError('No home')):
                # Should return original path instead of crashing
                self.assertEqual(setup._shorten_path('/home/user/docs'),
                                 '/home/user/docs')

    def test_print_skills_table(self):
        """Tests the formatting of the skills table."""
        skills = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            available=True,
                            installed=True,
                            enabled=True,
                            location='/root/src/loc1'),
            'skill2':
            setup.SkillInfo(name='skill2', available=True, installed=False),
            'skill3':
            setup.SkillInfo(name='skill3',
                            available=False,
                            installed=True,
                            location='/other/loc'),
            'skill4':
            setup.SkillInfo(name='skill4',
                            available=True,
                            installed=True,
                            enabled=True,
                            location='/home/user/loc4'),
        }

        expected_output = (
            'SKILL   AVAILABLE  INSTALLED  ENABLED  LOCATION  \n'
            '------  ---------  ---------  -------  ----------\n'
            'skill1  yes        yes        yes      //loc1    \n'
            'skill2  yes        no         no       -         \n'
            'skill3  no         yes        no       /other/loc\n'
            'skill4  yes        yes        yes      ~/loc4    \n')

        with unittest.mock.patch('sys.stdout',
                                 new_callable=io.StringIO) as mock_stdout:
            with (unittest.mock.patch('setup._PROJECT_ROOT',
                                      Path('/root/src')),
                  unittest.mock.patch('pathlib.Path.home',
                                      return_value=Path('/home/user'))):
                setup._print_skills_table(skills)
            self.assertEqual(mock_stdout.getvalue(), expected_output)

    def test_run_skill_command_enable(self):
        setup._run_skill_command('enable', 'test-skill')
        self.mock_run.assert_called_once_with(
            ['/path/to/gemini', 'skills', 'enable', 'test-skill'],
            check=True,
            capture_output=True,
            text=True)

    def test_run_skill_command_disable(self):
        setup._run_skill_command('disable', 'test-skill')
        self.mock_run.assert_called_once_with(
            ['/path/to/gemini', 'skills', 'disable', 'test-skill'],
            check=True,
            capture_output=True,
            text=True)

    @unittest.mock.patch('setup.get_available_skills')
    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_link(self, _mock_inst, mock_avail):
        project_root = Path('/root/src')
        self.fs.create_dir(project_root / '.agents' / 'skills')
        mock_avail.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            available=True,
                            path=project_root / 'skill1'),
            'skill2':
            setup.SkillInfo(name='skill2',
                            available=True,
                            path=project_root / 'skill2')
        }

        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1', 'skill2'])
            success = setup._handle_link(args)

        self.assertTrue(success)
        skills_dir = project_root / '.agents' / 'skills'
        self.assertTrue((skills_dir).is_dir())
        self.assertTrue((skills_dir / 'skill1').is_symlink())
        self.assertTrue((skills_dir / 'skill2').is_symlink())

    def test_handle_list(self):
        # Mock get_installed_skills and get_available_skills
        with (unittest.mock.patch('setup.get_installed_skills') as mock_inst,
              unittest.mock.patch('setup.get_available_skills') as mock_avail,
              unittest.mock.patch('setup._print_skills_table') as mock_print):

            mock_inst.return_value = {
                'skill1':
                setup.SkillInfo(name='skill1',
                                enabled=True,
                                installed=True,
                                location='/path/to/skill1')
            }
            mock_avail.return_value = {
                'skill1':
                setup.SkillInfo(name='skill1',
                                available=True,
                                path=Path('agents/skills/skill1'))
            }

            args = argparse.Namespace()
            success = setup._handle_list(args)

            self.assertTrue(success)
            mock_inst.assert_called_once()
            mock_avail.assert_called_once()

            # Verify that all_skills passed to _print_skills_table has both
            # available and installed set to True
            mock_print.assert_called_once()
            all_skills = mock_print.call_args[0][0]
            self.assertIn('skill1', all_skills)
            self.assertTrue(all_skills['skill1'].available)
            self.assertTrue(all_skills['skill1'].installed)
            self.assertTrue(all_skills['skill1'].enabled)
            self.assertEqual(all_skills['skill1'].location, '/path/to/skill1')

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_uninstall_symlink(self, mock_inst):
        project_root = Path('/root/src')
        skill_loc = project_root / '.agents' / 'skills' / 'skill1'
        self.fs.create_dir(project_root / '.agents' / 'skills')
        self.fs.create_dir(project_root / 'skill1_src')
        os.symlink(project_root / 'skill1_src', skill_loc)

        mock_inst.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            installed=True,
                            location=str(skill_loc))
        }
        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1'])
            success = setup._handle_uninstall(args)
        self.assertTrue(success)
        self.assertFalse(skill_loc.exists())
        # Should NOT call subprocess uninstall if it's a symlink
        self.mock_run.assert_not_called()

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_uninstall_dir(self, mock_inst):
        project_root = Path('/root/src')
        skill_loc = project_root / '.agents' / 'skills' / 'skill1'
        self.fs.create_dir(skill_loc)

        mock_inst.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            installed=True,
                            location=str(skill_loc))
        }
        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1'])
            success = setup._handle_uninstall(args)
        self.assertTrue(success)
        # Directory still exists because setup.py doesn't delete it (gemini
        # does, but we mock it).
        self.assertTrue(skill_loc.exists())
        # Should call subprocess uninstall if it's NOT a symlink
        self.mock_run.assert_called_once_with(
            self.gemini_cmd + ['skills', 'uninstall', 'skill1'],
            check=True,
            capture_output=True,
            text=True)

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_enable(self, mock_inst):
        mock_inst.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            location='/path/to/skill',
                            installed=True)
        }
        args = argparse.Namespace(names=['skill1'])
        success = setup._handle_enable(args)
        self.assertTrue(success)
        self.mock_run.assert_called_once_with(
            ['/path/to/gemini', 'skills', 'enable', 'skill1'],
            check=True,
            capture_output=True,
            text=True)

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_disable(self, mock_inst):
        mock_inst.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            location='/path/to/skill',
                            installed=True)
        }
        args = argparse.Namespace(names=['skill1'])
        success = setup._handle_disable(args)
        self.assertTrue(success)
        self.mock_run.assert_called_once_with(
            ['/path/to/gemini', 'skills', 'disable', 'skill1'],
            check=True,
            capture_output=True,
            text=True)

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_enable_skill_not_found(self, mock_inst):
        mock_inst.return_value = {}
        args = argparse.Namespace(names=['unknown-skill'])
        success = setup._handle_enable(args)
        self.assertFalse(success)
        self.mock_run.assert_not_called()

    def test_get_installed_skills_regex_robustness(self):
        # Test with CRLF and trailing spaces
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout=("skill-1 [Enabled]  \n"
                    "  Description: desc 1 \n"
                    "  Location: loc1/SKILL.md \n"),
            returncode=0)
        skills = setup.get_installed_skills()
        self.assertEqual(len(skills), 1)
        # Verify it was stripped
        self.assertEqual(skills['skill-1'].location, 'loc1')

    def test_get_available_skills_missing_dirs(self):
        project_root = Path('/root')
        # Only one dir exists
        skill1_dir = project_root / 'agents' / 'skills' / 'skill1'
        self.fs.create_dir(skill1_dir)
        self.fs.create_file(skill1_dir / 'SKILL.md')

        # Mock SKILL_DIRS to include a non-existent directory
        with unittest.mock.patch('setup.SKILL_DIRS', [
                project_root / 'agents' / 'skills',
                project_root / 'non-existent'
        ]):
            available = setup.get_available_skills()

        self.assertEqual(len(available), 1)
        self.assertIn('skill1', available)

    def test_get_available_skills_duplicates(self):
        project_root = Path('/root')
        dir1 = project_root / 'dir1'
        dir2 = project_root / 'dir2'

        skill_v1 = dir1 / 'my-skill'
        self.fs.create_dir(skill_v1)
        self.fs.create_file(skill_v1 / 'SKILL.md')

        skill_v2 = dir2 / 'my-skill'
        self.fs.create_dir(skill_v2)
        self.fs.create_file(skill_v2 / 'SKILL.md')

        # Later directories should overwrite earlier ones
        with unittest.mock.patch('setup.SKILL_DIRS', [dir1, dir2]):
            available = setup.get_available_skills()

        self.assertEqual(len(available), 1)
        # Should be the one from dir2
        self.assertEqual(available['my-skill'].path, skill_v2)

    @unittest.mock.patch('setup.get_available_skills')
    def test_handle_link_collision(self, mock_avail):
        project_root = Path('/root/src')
        link_path = project_root / '.agents' / 'skills' / 'skill1'
        self.fs.create_dir(project_root / '.agents' / 'skills')
        self.fs.create_file(link_path)  # File already exists

        mock_avail.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            available=True,
                            path=project_root / 'skill1')
        }

        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1'])
            success = setup._handle_link(args)
        self.assertTrue(success)

    @unittest.mock.patch('setup.get_installed_skills')
    def test_handle_uninstall_missing_path(self, mock_inst):
        project_root = Path('/root/src')
        skill_loc = project_root / '.agents' / 'skills' / 'skill1'
        # Reported as installed, but loc doesn't exist on disk

        mock_inst.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            installed=True,
                            location=str(skill_loc))
        }
        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1'])
            success = setup._handle_uninstall(args)
        self.assertTrue(success)

    @unittest.mock.patch('setup.get_available_skills')
    def test_handle_link_creates_parent_dir(self, mock_avail):
        project_root = Path('/root/src')
        # .agents/skills does NOT exist

        mock_avail.return_value = {
            'skill1':
            setup.SkillInfo(name='skill1',
                            available=True,
                            path=project_root / 'skill1')
        }

        with unittest.mock.patch('setup._PROJECT_ROOT', project_root):
            args = argparse.Namespace(names=['skill1'])
            success = setup._handle_link(args)

        self.assertTrue(success)
        self.assertTrue((project_root / '.agents' / 'skills').is_dir())
        self.assertTrue(
            (project_root / '.agents' / 'skills' / 'skill1').is_symlink())

    def test_get_installed_skills_malformed_output(self):
        # Output that doesn't match the regex
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout="Some unrelated debugging output\n"
            "Totally not a skill block\n",
            returncode=0)
        skills = setup.get_installed_skills()
        self.assertEqual(len(skills), 0)

    def test_get_installed_skills_empty_description(self):
        self.mock_run.return_value = unittest.mock.MagicMock(
            stdout=("skill-1 [Enabled]\n"
                    "  Description: \n"
                    "  Location: loc1/SKILL.md\n"),
            returncode=0)
        skills = setup.get_installed_skills()
        self.assertEqual(len(skills), 1)
        self.assertEqual(skills['skill-1'].location, 'loc1')


if __name__ == '__main__':
    unittest.main()
